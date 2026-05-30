#include "Storage.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace local_jarvis::storage {
namespace {

constexpr const char *kSchemaVersion = "1";

constexpr const char *kCoreSchemaSql = R"sql(
CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS sessions (
    id TEXT PRIMARY KEY,
    mode TEXT NOT NULL,
    title TEXT,
    started_at TEXT NOT NULL,
    ended_at TEXT,
    summary_status TEXT DEFAULT 'pending'
);

CREATE TABLE IF NOT EXISTS transcript_segments (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    start_ms INTEGER NOT NULL,
    end_ms INTEGER NOT NULL,
    speaker TEXT,
    text TEXT NOT NULL,
    source TEXT NOT NULL,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS screen_ocr_segments (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    timestamp_ms INTEGER NOT NULL,
    window_title TEXT,
    text TEXT NOT NULL,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS processed_notes (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    type TEXT NOT NULL,
    title TEXT,
    body TEXT NOT NULL,
    json_body TEXT,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS action_items (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    text TEXT NOT NULL,
    status TEXT DEFAULT 'open',
    due_at TEXT,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS flashcards (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    question TEXT NOT NULL,
    answer TEXT NOT NULL,
    topic TEXT,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS model_events (
    id TEXT PRIMARY KEY,
    event_type TEXT NOT NULL,
    model_name TEXT,
    details TEXT,
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS privacy_events (
    id TEXT PRIMARY KEY,
    session_id TEXT,
    event_type TEXT NOT NULL,
    details TEXT NOT NULL,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_transcript_segments_session_start
    ON transcript_segments(session_id, start_ms);

CREATE INDEX IF NOT EXISTS idx_screen_ocr_segments_session_time
    ON screen_ocr_segments(session_id, timestamp_ms);

CREATE INDEX IF NOT EXISTS idx_processed_notes_session_type
    ON processed_notes(session_id, type);

CREATE INDEX IF NOT EXISTS idx_action_items_session_status
    ON action_items(session_id, status);

CREATE INDEX IF NOT EXISTS idx_flashcards_session_topic
    ON flashcards(session_id, topic);

CREATE INDEX IF NOT EXISTS idx_model_events_created_at
    ON model_events(created_at);

CREATE INDEX IF NOT EXISTS idx_privacy_events_session_created
    ON privacy_events(session_id, created_at);

CREATE INDEX IF NOT EXISTS idx_sessions_started_at
    ON sessions(started_at);
)sql";

std::string pathToUtf8(const std::filesystem::path &path)
{
    const auto value = path.u8string();
    return { reinterpret_cast<const char *>(value.data()), value.size() };
}

std::filesystem::path homePath()
{
    if (const char *home = std::getenv("HOME")) {
        return home;
    }

#if defined(_WIN32)
    if (const char *profile = std::getenv("USERPROFILE")) {
        return profile;
    }
#endif

    return std::filesystem::current_path();
}

std::string utcNow()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc {};

#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif

    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

std::string generateId(const char *prefix)
{
    static thread_local std::mt19937_64 rng { std::random_device {}() };
    std::uniform_int_distribution<unsigned long long> distribution;

    std::ostringstream stream;
    stream << prefix << "-"
           << std::hex << std::setw(16) << std::setfill('0') << distribution(rng)
           << std::setw(16) << std::setfill('0') << distribution(rng);
    return stream.str();
}

const char *columnText(sqlite3_stmt *statement, int column)
{
    const auto *text = sqlite3_column_text(statement, column);
    return text == nullptr ? "" : reinterpret_cast<const char *>(text);
}

std::optional<std::string> nullableColumnText(sqlite3_stmt *statement, int column)
{
    if (sqlite3_column_type(statement, column) == SQLITE_NULL) {
        return std::nullopt;
    }

    return std::string(columnText(statement, column));
}

void bindText(sqlite3_stmt *statement, int index, const std::string &value)
{
    sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void bindOptionalText(sqlite3_stmt *statement, int index, const std::optional<std::string> &value)
{
    if (value.has_value()) {
        bindText(statement, index, *value);
        return;
    }

    sqlite3_bind_null(statement, index);
}

std::string likePattern(const std::string &query)
{
    return "%" + query + "%";
}

std::string ftsPhrase(const std::string &query)
{
    std::string escaped = "\"";
    for (const char character : query) {
        if (character == '"') {
            escaped += "\"\"";
        } else {
            escaped += character;
        }
    }
    escaped += '"';
    return escaped;
}

} // namespace

Storage::~Storage()
{
    close();
}

std::filesystem::path Storage::defaultDatabasePath()
{
#if defined(_WIN32)
    if (const char *localAppData = std::getenv("LOCALAPPDATA")) {
        return std::filesystem::path(localAppData) / "LocalJarvis" / "data" / "local_jarvis.db";
    }
    return homePath() / "AppData" / "Local" / "LocalJarvis" / "data" / "local_jarvis.db";
#elif defined(__APPLE__)
    return homePath() / "Library" / "Application Support" / "LocalJarvis" / "data" / "local_jarvis.db";
#else
    return homePath() / ".local" / "share" / "local-jarvis" / "data" / "local_jarvis.db";
#endif
}

bool Storage::initialize()
{
    std::lock_guard lock(m_mutex);
    return initialize(defaultDatabasePath());
}

bool Storage::initialize(const std::filesystem::path &databasePath)
{
    std::lock_guard lock(m_mutex);
    if (!open(databasePath)) {
        return false;
    }

    return runMigrations();
}

bool Storage::open()
{
    std::lock_guard lock(m_mutex);
    return open(defaultDatabasePath());
}

bool Storage::open(const std::filesystem::path &databasePath)
{
    std::lock_guard lock(m_mutex);
    close();

    if (databasePath.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(databasePath.parent_path(), error);
        if (error) {
            m_lastError = "Failed to create database directory: " + error.message();
            return false;
        }
    }

    const std::string path = pathToUtf8(databasePath);
    const int result = sqlite3_open_v2(
        path.c_str(),
        &m_database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);

    if (result != SQLITE_OK) {
        setLastSqliteError("Failed to open SQLite database");
        close();
        return false;
    }

    if (!execute("PRAGMA foreign_keys = ON;")) {
        return false;
    }

    m_lastError.clear();
    return true;
}

void Storage::close()
{
    std::lock_guard lock(m_mutex);
    if (m_database != nullptr) {
        sqlite3_close(m_database);
        m_database = nullptr;
    }
    m_ftsAvailable = false;
}

bool Storage::createSchema()
{
    std::lock_guard lock(m_mutex);
    return runMigrations();
}

bool Storage::initializeSchema()
{
    std::lock_guard lock(m_mutex);
    return runMigrations();
}

std::optional<std::string> Storage::createSession(
    const std::string &mode,
    const std::optional<std::string> &title)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return std::nullopt;
    }

    constexpr const char *sql = R"sql(
INSERT INTO sessions (id, mode, title, started_at, summary_status)
VALUES (?, ?, ?, ?, 'pending');
)sql";
    sqlite3_stmt *statement = nullptr;

    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare create session statement");
        return std::nullopt;
    }

    const std::string sessionId = generateId("session");
    const std::string startedAt = utcNow();
    const std::string storedMode = mode.empty() ? "manual" : mode;

    bindText(statement, 1, sessionId);
    bindText(statement, 2, storedMode);
    bindOptionalText(statement, 3, title);
    bindText(statement, 4, startedAt);

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);
    if (!ok) {
        return std::nullopt;
    }

    return sessionId;
}

bool Storage::endSession(const std::string &sessionId)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return false;
    }

    constexpr const char *sql = "UPDATE sessions SET ended_at = ? WHERE id = ?;";
    sqlite3_stmt *statement = nullptr;

    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare finish session statement");
        return false;
    }

    bindText(statement, 1, utcNow());
    bindText(statement, 2, sessionId);

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);

    if (ok && sqlite3_changes(m_database) == 0) {
        m_lastError = "No session row matched id " + sessionId;
        return false;
    }

    return ok;
}

bool Storage::setSessionSummaryStatus(const std::string &sessionId, const std::string &summaryStatus)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return false;
    }

    constexpr const char *sql = "UPDATE sessions SET summary_status = ? WHERE id = ?;";
    sqlite3_stmt *statement = nullptr;

    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare update session summary status statement");
        return false;
    }

    bindText(statement, 1, summaryStatus.empty() ? "pending" : summaryStatus);
    bindText(statement, 2, sessionId);

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);

    if (ok && sqlite3_changes(m_database) == 0) {
        m_lastError = "No session row matched id " + sessionId;
        return false;
    }

    return ok;
}

std::optional<std::string> Storage::addTranscriptSegment(const TranscriptSegmentInput &segment)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return std::nullopt;
    }

    constexpr const char *sql = R"sql(
INSERT INTO transcript_segments (id, session_id, start_ms, end_ms, speaker, text, source, created_at)
VALUES (?, ?, ?, ?, ?, ?, ?, ?);
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare add transcript segment statement");
        return std::nullopt;
    }

    const std::string id = generateId("transcript");
    bindText(statement, 1, id);
    bindText(statement, 2, segment.sessionId);
    sqlite3_bind_int64(statement, 3, segment.startMs);
    sqlite3_bind_int64(statement, 4, segment.endMs);
    bindOptionalText(statement, 5, segment.speaker);
    bindText(statement, 6, segment.text);
    bindText(statement, 7, segment.source);
    bindText(statement, 8, utcNow());

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);
    if (!ok) {
        return std::nullopt;
    }

    if (m_ftsAvailable) {
        sqlite3_stmt *ftsStatement = nullptr;
        constexpr const char *ftsSql = "INSERT INTO transcript_fts (id, session_id, text) VALUES (?, ?, ?);";
        if (sqlite3_prepare_v2(m_database, ftsSql, -1, &ftsStatement, nullptr) == SQLITE_OK) {
            bindText(ftsStatement, 1, id);
            bindText(ftsStatement, 2, segment.sessionId);
            bindText(ftsStatement, 3, segment.text);
            sqlite3_step(ftsStatement);
            sqlite3_finalize(ftsStatement);
        }
    }

    m_lastError.clear();
    return id;
}

bool Storage::deleteTranscriptSegment(const std::string &segmentId)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return false;
    }

    constexpr const char *sql = "DELETE FROM transcript_segments WHERE id = ?;";
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare delete transcript segment statement");
        return false;
    }

    bindText(statement, 1, segmentId);
    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);
    if (!ok) {
        return false;
    }

    if (m_ftsAvailable) {
        sqlite3_stmt *ftsStatement = nullptr;
        constexpr const char *ftsSql = "DELETE FROM transcript_fts WHERE id = ?;";
        if (sqlite3_prepare_v2(m_database, ftsSql, -1, &ftsStatement, nullptr) == SQLITE_OK) {
            bindText(ftsStatement, 1, segmentId);
            sqlite3_step(ftsStatement);
            sqlite3_finalize(ftsStatement);
        }
    }

    m_lastError.clear();
    return true;
}

std::optional<std::string> Storage::addScreenOcrSegment(const ScreenOcrSegmentInput &segment)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return std::nullopt;
    }

    constexpr const char *sql = R"sql(
INSERT INTO screen_ocr_segments (id, session_id, timestamp_ms, window_title, text, created_at)
VALUES (?, ?, ?, ?, ?, ?);
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare add screen OCR segment statement");
        return std::nullopt;
    }

    const std::string id = generateId("screen-ocr");
    bindText(statement, 1, id);
    bindText(statement, 2, segment.sessionId);
    sqlite3_bind_int64(statement, 3, segment.timestampMs);
    bindOptionalText(statement, 4, segment.windowTitle);
    bindText(statement, 5, segment.text);
    bindText(statement, 6, utcNow());

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);
    if (!ok) {
        return std::nullopt;
    }

    return id;
}

std::optional<std::string> Storage::addProcessedNote(const ProcessedNoteInput &note)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return std::nullopt;
    }

    constexpr const char *sql = R"sql(
INSERT INTO processed_notes (id, session_id, type, title, body, json_body, created_at)
VALUES (?, ?, ?, ?, ?, ?, ?);
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare add processed note statement");
        return std::nullopt;
    }

    const std::string id = generateId("note");
    bindText(statement, 1, id);
    bindText(statement, 2, note.sessionId);
    bindText(statement, 3, note.type);
    bindOptionalText(statement, 4, note.title);
    bindText(statement, 5, note.body);
    bindOptionalText(statement, 6, note.jsonBody);
    bindText(statement, 7, utcNow());

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);
    if (!ok) {
        return std::nullopt;
    }

    if (m_ftsAvailable) {
        sqlite3_stmt *ftsStatement = nullptr;
        constexpr const char *ftsSql = "INSERT INTO processed_notes_fts (id, session_id, title, body) VALUES (?, ?, ?, ?);";
        if (sqlite3_prepare_v2(m_database, ftsSql, -1, &ftsStatement, nullptr) == SQLITE_OK) {
            bindText(ftsStatement, 1, id);
            bindText(ftsStatement, 2, note.sessionId);
            bindText(ftsStatement, 3, note.title.value_or(""));
            bindText(ftsStatement, 4, note.body);
            sqlite3_step(ftsStatement);
            sqlite3_finalize(ftsStatement);
        }
    }

    m_lastError.clear();
    return id;
}

std::optional<std::string> Storage::addActionItem(const ActionItemInput &actionItem)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return std::nullopt;
    }

    constexpr const char *sql = R"sql(
INSERT INTO action_items (id, session_id, text, status, due_at, created_at)
VALUES (?, ?, ?, ?, ?, ?);
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare add action item statement");
        return std::nullopt;
    }

    const std::string id = generateId("action");
    bindText(statement, 1, id);
    bindText(statement, 2, actionItem.sessionId);
    bindText(statement, 3, actionItem.text);
    bindText(statement, 4, actionItem.status.empty() ? "open" : actionItem.status);
    bindOptionalText(statement, 5, actionItem.dueAt);
    bindText(statement, 6, utcNow());

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);
    if (!ok) {
        return std::nullopt;
    }

    return id;
}

std::optional<std::string> Storage::addFlashcard(const FlashcardInput &flashcard)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return std::nullopt;
    }

    constexpr const char *sql = R"sql(
INSERT INTO flashcards (id, session_id, question, answer, topic, created_at)
VALUES (?, ?, ?, ?, ?, ?);
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare add flashcard statement");
        return std::nullopt;
    }

    const std::string id = generateId("flashcard");
    bindText(statement, 1, id);
    bindText(statement, 2, flashcard.sessionId);
    bindText(statement, 3, flashcard.question);
    bindText(statement, 4, flashcard.answer);
    bindOptionalText(statement, 5, flashcard.topic);
    bindText(statement, 6, utcNow());

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);
    if (!ok) {
        return std::nullopt;
    }

    return id;
}

std::vector<SessionRecord> Storage::listRecentSessions(int limit)
{
    std::lock_guard lock(m_mutex);
    std::vector<SessionRecord> sessions;

    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return sessions;
    }

    constexpr const char *sql = R"sql(
SELECT id, mode, started_at, ended_at, title, COALESCE(summary_status, 'pending')
FROM sessions
ORDER BY started_at DESC
LIMIT ?;
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare list recent sessions statement");
        return sessions;
    }

    sqlite3_bind_int(statement, 1, limit < 1 ? 1 : limit);

    while (true) {
        const int result = sqlite3_step(statement);
        if (result == SQLITE_DONE) {
            m_lastError.clear();
            break;
        }

        if (result != SQLITE_ROW) {
            setLastSqliteError("Failed to list recent sessions");
            break;
        }

        sessions.push_back(SessionRecord {
            .id = columnText(statement, 0),
            .mode = columnText(statement, 1),
            .startedAt = columnText(statement, 2),
            .endedAt = nullableColumnText(statement, 3),
            .title = nullableColumnText(statement, 4),
            .summaryStatus = columnText(statement, 5)
        });
    }

    sqlite3_finalize(statement);
    return sessions;
}

std::vector<TranscriptSegmentRecord> Storage::listRecentTranscriptSegments(const std::string &sessionId, int limit)
{
    std::lock_guard lock(m_mutex);
    std::vector<TranscriptSegmentRecord> segments;
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return segments;
    }

    constexpr const char *sql = R"sql(
SELECT id, session_id, start_ms, end_ms, speaker, text, source, created_at
FROM (
    SELECT id, session_id, start_ms, end_ms, speaker, text, source, created_at
    FROM transcript_segments
    WHERE session_id = ?
    ORDER BY start_ms DESC
    LIMIT ?
)
ORDER BY start_ms ASC;
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare list recent transcript segments statement");
        return segments;
    }

    bindText(statement, 1, sessionId);
    sqlite3_bind_int(statement, 2, limit < 1 ? 1 : limit);

    while (sqlite3_step(statement) == SQLITE_ROW) {
        segments.push_back(TranscriptSegmentRecord {
            .id = columnText(statement, 0),
            .sessionId = columnText(statement, 1),
            .startMs = sqlite3_column_int64(statement, 2),
            .endMs = sqlite3_column_int64(statement, 3),
            .speaker = nullableColumnText(statement, 4),
            .text = columnText(statement, 5),
            .source = columnText(statement, 6),
            .createdAt = columnText(statement, 7)
        });
    }

    sqlite3_finalize(statement);
    m_lastError.clear();
    return segments;
}

std::vector<ScreenOcrSegmentRecord> Storage::listRecentScreenOcrSegments(const std::string &sessionId, int limit)
{
    std::lock_guard lock(m_mutex);
    std::vector<ScreenOcrSegmentRecord> segments;
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return segments;
    }

    constexpr const char *sql = R"sql(
SELECT id, session_id, timestamp_ms, window_title, text, created_at
FROM (
    SELECT id, session_id, timestamp_ms, window_title, text, created_at
    FROM screen_ocr_segments
    WHERE session_id = ?
    ORDER BY timestamp_ms DESC
    LIMIT ?
)
ORDER BY timestamp_ms ASC;
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare list recent screen OCR segments statement");
        return segments;
    }

    bindText(statement, 1, sessionId);
    sqlite3_bind_int(statement, 2, limit < 1 ? 1 : limit);

    while (sqlite3_step(statement) == SQLITE_ROW) {
        segments.push_back(ScreenOcrSegmentRecord {
            .id = columnText(statement, 0),
            .sessionId = columnText(statement, 1),
            .timestampMs = sqlite3_column_int64(statement, 2),
            .windowTitle = nullableColumnText(statement, 3),
            .text = columnText(statement, 4),
            .createdAt = columnText(statement, 5)
        });
    }

    sqlite3_finalize(statement);
    m_lastError.clear();
    return segments;
}

std::vector<ProcessedNoteRecord> Storage::listLatestProcessedNotes(const std::string &sessionId, int limit)
{
    std::lock_guard lock(m_mutex);
    std::vector<ProcessedNoteRecord> notes;
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return notes;
    }

    constexpr const char *sql = R"sql(
SELECT id, session_id, type, title, body, json_body, created_at
FROM processed_notes
WHERE session_id = ?
ORDER BY created_at DESC
LIMIT ?;
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare list processed notes statement");
        return notes;
    }

    bindText(statement, 1, sessionId);
    sqlite3_bind_int(statement, 2, limit < 1 ? 1 : limit);

    while (sqlite3_step(statement) == SQLITE_ROW) {
        notes.push_back(ProcessedNoteRecord {
            .id = columnText(statement, 0),
            .sessionId = columnText(statement, 1),
            .type = columnText(statement, 2),
            .title = nullableColumnText(statement, 3),
            .body = columnText(statement, 4),
            .jsonBody = nullableColumnText(statement, 5),
            .createdAt = columnText(statement, 6)
        });
    }

    sqlite3_finalize(statement);
    m_lastError.clear();
    return notes;
}

std::vector<ActionItemRecord> Storage::listActionItems(const std::string &sessionId, int limit)
{
    std::lock_guard lock(m_mutex);
    std::vector<ActionItemRecord> actionItems;
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return actionItems;
    }

    constexpr const char *sql = R"sql(
SELECT id, session_id, text, COALESCE(status, 'open'), due_at, created_at
FROM action_items
WHERE session_id = ?
ORDER BY created_at DESC
LIMIT ?;
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare list action items statement");
        return actionItems;
    }

    bindText(statement, 1, sessionId);
    sqlite3_bind_int(statement, 2, limit < 1 ? 1 : limit);

    while (sqlite3_step(statement) == SQLITE_ROW) {
        actionItems.push_back(ActionItemRecord {
            .id = columnText(statement, 0),
            .sessionId = columnText(statement, 1),
            .text = columnText(statement, 2),
            .status = columnText(statement, 3),
            .dueAt = nullableColumnText(statement, 4),
            .createdAt = columnText(statement, 5)
        });
    }

    sqlite3_finalize(statement);
    m_lastError.clear();
    return actionItems;
}

std::vector<FlashcardRecord> Storage::listFlashcards(const std::string &sessionId, int limit)
{
    std::lock_guard lock(m_mutex);
    std::vector<FlashcardRecord> flashcards;
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return flashcards;
    }

    constexpr const char *sql = R"sql(
SELECT id, session_id, question, answer, topic, created_at
FROM flashcards
WHERE session_id = ?
ORDER BY created_at DESC
LIMIT ?;
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare list flashcards statement");
        return flashcards;
    }

    bindText(statement, 1, sessionId);
    sqlite3_bind_int(statement, 2, limit < 1 ? 1 : limit);

    while (sqlite3_step(statement) == SQLITE_ROW) {
        flashcards.push_back(FlashcardRecord {
            .id = columnText(statement, 0),
            .sessionId = columnText(statement, 1),
            .question = columnText(statement, 2),
            .answer = columnText(statement, 3),
            .topic = nullableColumnText(statement, 4),
            .createdAt = columnText(statement, 5)
        });
    }

    sqlite3_finalize(statement);
    m_lastError.clear();
    return flashcards;
}

int Storage::countTranscriptSegmentsForSession(const std::string &sessionId)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return -1;
    }

    constexpr const char *sql = "SELECT COUNT(*) FROM transcript_segments WHERE session_id = ?;";
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare count transcript segments statement");
        return -1;
    }

    bindText(statement, 1, sessionId);

    int count = -1;
    const int result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        count = sqlite3_column_int(statement, 0);
        m_lastError.clear();
    } else {
        setLastSqliteError("Failed to count transcript segments");
    }

    sqlite3_finalize(statement);
    return count;
}

bool Storage::setSetting(const std::string &key, const std::string &value)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return false;
    }

    constexpr const char *sql = R"sql(
INSERT INTO settings (key, value, updated_at)
VALUES (?, ?, ?)
ON CONFLICT(key) DO UPDATE SET
    value = excluded.value,
    updated_at = excluded.updated_at;
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare set setting statement");
        return false;
    }

    bindText(statement, 1, key);
    bindText(statement, 2, value);
    bindText(statement, 3, utcNow());

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);
    return ok;
}

std::optional<std::string> Storage::getSetting(const std::string &key)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return std::nullopt;
    }

    constexpr const char *sql = "SELECT value FROM settings WHERE key = ?;";
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare get setting statement");
        return std::nullopt;
    }

    bindText(statement, 1, key);

    std::optional<std::string> value;
    const int result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        value = columnText(statement, 0);
        m_lastError.clear();
    } else if (result == SQLITE_DONE) {
        m_lastError.clear();
    } else {
        setLastSqliteError("Failed to read setting");
    }

    sqlite3_finalize(statement);
    return value;
}

std::optional<std::string> Storage::addModelEvent(
    const std::string &eventType,
    const std::optional<std::string> &modelName,
    const std::optional<std::string> &details)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return std::nullopt;
    }

    constexpr const char *sql = R"sql(
INSERT INTO model_events (id, event_type, model_name, details, created_at)
VALUES (?, ?, ?, ?, ?);
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare add model event statement");
        return std::nullopt;
    }

    const std::string id = generateId("model-event");
    bindText(statement, 1, id);
    bindText(statement, 2, eventType);
    bindOptionalText(statement, 3, modelName);
    bindOptionalText(statement, 4, details);
    bindText(statement, 5, utcNow());

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);
    if (!ok) {
        return std::nullopt;
    }

    return id;
}

std::optional<std::string> Storage::addPrivacyEvent(const PrivacyEventInput &event)
{
    std::lock_guard lock(m_mutex);
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return std::nullopt;
    }

    constexpr const char *sql = R"sql(
INSERT INTO privacy_events (id, session_id, event_type, details, created_at)
VALUES (?, ?, ?, ?, ?);
)sql";

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(m_database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to prepare add privacy event statement");
        return std::nullopt;
    }

    const std::string id = generateId("privacy-event");
    bindText(statement, 1, id);
    bindOptionalText(statement, 2, event.sessionId);
    bindText(statement, 3, event.eventType);
    bindText(statement, 4, event.details);
    bindText(statement, 5, utcNow());

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);
    if (!ok) {
        return std::nullopt;
    }

    return id;
}

std::vector<SearchResult> Storage::searchTranscripts(const std::string &query, int limit)
{
    std::lock_guard lock(m_mutex);
    std::vector<SearchResult> results;
    if (!isOpen() || query.empty()) {
        return results;
    }

    sqlite3_stmt *statement = nullptr;
    if (m_ftsAvailable) {
        constexpr const char *ftsSql = R"sql(
SELECT id, session_id, text, bm25(transcript_fts) AS rank
FROM transcript_fts
WHERE transcript_fts MATCH ?
ORDER BY rank
LIMIT ?;
)sql";
        if (sqlite3_prepare_v2(m_database, ftsSql, -1, &statement, nullptr) == SQLITE_OK) {
            bindText(statement, 1, ftsPhrase(query));
            sqlite3_bind_int(statement, 2, limit < 1 ? 1 : limit);
        }
    }

    if (statement == nullptr) {
        constexpr const char *likeSql = R"sql(
SELECT id, session_id, text, 0.0
FROM transcript_segments
WHERE text LIKE ?
ORDER BY start_ms
LIMIT ?;
)sql";
        if (sqlite3_prepare_v2(m_database, likeSql, -1, &statement, nullptr) != SQLITE_OK) {
            setLastSqliteError("Failed to prepare transcript search statement");
            return results;
        }
        bindText(statement, 1, likePattern(query));
        sqlite3_bind_int(statement, 2, limit < 1 ? 1 : limit);
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        results.push_back(SearchResult {
            .id = columnText(statement, 0),
            .sessionId = columnText(statement, 1),
            .text = columnText(statement, 2),
            .rank = sqlite3_column_double(statement, 3)
        });
    }

    sqlite3_finalize(statement);
    m_lastError.clear();
    return results;
}

std::vector<SearchResult> Storage::searchProcessedNotes(const std::string &query, int limit)
{
    std::lock_guard lock(m_mutex);
    std::vector<SearchResult> results;
    if (!isOpen() || query.empty()) {
        return results;
    }

    sqlite3_stmt *statement = nullptr;
    if (m_ftsAvailable) {
        constexpr const char *ftsSql = R"sql(
SELECT id, session_id, COALESCE(title, '') || char(10) || body, bm25(processed_notes_fts) AS rank
FROM processed_notes_fts
WHERE processed_notes_fts MATCH ?
ORDER BY rank
LIMIT ?;
)sql";
        if (sqlite3_prepare_v2(m_database, ftsSql, -1, &statement, nullptr) == SQLITE_OK) {
            bindText(statement, 1, ftsPhrase(query));
            sqlite3_bind_int(statement, 2, limit < 1 ? 1 : limit);
        }
    }

    if (statement == nullptr) {
        constexpr const char *likeSql = R"sql(
SELECT id, session_id, COALESCE(title, '') || char(10) || body, 0.0
FROM processed_notes
WHERE title LIKE ? OR body LIKE ?
ORDER BY created_at DESC
LIMIT ?;
)sql";
        if (sqlite3_prepare_v2(m_database, likeSql, -1, &statement, nullptr) != SQLITE_OK) {
            setLastSqliteError("Failed to prepare processed note search statement");
            return results;
        }
        const std::string pattern = likePattern(query);
        bindText(statement, 1, pattern);
        bindText(statement, 2, pattern);
        sqlite3_bind_int(statement, 3, limit < 1 ? 1 : limit);
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        results.push_back(SearchResult {
            .id = columnText(statement, 0),
            .sessionId = columnText(statement, 1),
            .text = columnText(statement, 2),
            .rank = sqlite3_column_double(statement, 3)
        });
    }

    sqlite3_finalize(statement);
    m_lastError.clear();
    return results;
}

bool Storage::isOpen() const
{
    std::lock_guard lock(m_mutex);
    return m_database != nullptr;
}

std::string Storage::lastError() const
{
    std::lock_guard lock(m_mutex);
    return m_lastError;
}

bool Storage::runMigrations()
{
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return false;
    }

    if (!ensureCoreSchema()) {
        return false;
    }

    if (!addColumnIfMissing("sessions", "summary_status", "summary_status TEXT DEFAULT 'pending'")
        || !addColumnIfMissing("transcript_segments", "created_at", "created_at TEXT NOT NULL DEFAULT '1970-01-01T00:00:00Z'")
        || !addColumnIfMissing("screen_ocr_segments", "created_at", "created_at TEXT NOT NULL DEFAULT '1970-01-01T00:00:00Z'")
        || !addColumnIfMissing("processed_notes", "json_body", "json_body TEXT")
        || !addColumnIfMissing("action_items", "created_at", "created_at TEXT NOT NULL DEFAULT '1970-01-01T00:00:00Z'")
        || !addColumnIfMissing("flashcards", "created_at", "created_at TEXT NOT NULL DEFAULT '1970-01-01T00:00:00Z'")) {
        return false;
    }

    if (!ensureFtsTables()) {
        return false;
    }

    return setSetting("schema_version", kSchemaVersion);
}

bool Storage::ensureCoreSchema()
{
    return execute(kCoreSchemaSql);
}

bool Storage::ensureFtsTables()
{
    m_ftsAvailable = isFts5Available();
    if (!m_ftsAvailable) {
        m_lastError.clear();
        return setSetting("features.fts5", "false");
    }

    constexpr const char *ftsSql = R"sql(
CREATE VIRTUAL TABLE IF NOT EXISTS transcript_fts
USING fts5(id UNINDEXED, session_id UNINDEXED, text);

CREATE VIRTUAL TABLE IF NOT EXISTS processed_notes_fts
USING fts5(id UNINDEXED, session_id UNINDEXED, title, body);

INSERT INTO transcript_fts (id, session_id, text)
SELECT t.id, t.session_id, t.text
FROM transcript_segments t
WHERE NOT EXISTS (
    SELECT 1 FROM transcript_fts f WHERE f.id = t.id
);

INSERT INTO processed_notes_fts (id, session_id, title, body)
SELECT p.id, p.session_id, COALESCE(p.title, ''), p.body
FROM processed_notes p
WHERE NOT EXISTS (
    SELECT 1 FROM processed_notes_fts f WHERE f.id = p.id
);
)sql";

    if (!execute(ftsSql)) {
        return false;
    }

    return setSetting("features.fts5", "true");
}

bool Storage::isFts5Available()
{
    char *errorMessage = nullptr;
    const int createResult = sqlite3_exec(
        m_database,
        "CREATE VIRTUAL TABLE IF NOT EXISTS temp.local_jarvis_fts5_probe USING fts5(content);",
        nullptr,
        nullptr,
        &errorMessage);

    if (createResult != SQLITE_OK) {
        sqlite3_free(errorMessage);
        m_lastError.clear();
        return false;
    }

    sqlite3_exec(m_database, "DROP TABLE IF EXISTS temp.local_jarvis_fts5_probe;", nullptr, nullptr, nullptr);
    return true;
}

bool Storage::addColumnIfMissing(
    const std::string &tableName,
    const std::string &columnName,
    const std::string &columnDefinition)
{
    if (columnExists(tableName, columnName)) {
        return true;
    }

    return execute("ALTER TABLE " + tableName + " ADD COLUMN " + columnDefinition + ";");
}

bool Storage::columnExists(const std::string &tableName, const std::string &columnName)
{
    sqlite3_stmt *statement = nullptr;
    const std::string sql = "PRAGMA table_info(" + tableName + ");";
    if (sqlite3_prepare_v2(m_database, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        setLastSqliteError("Failed to inspect table columns");
        return false;
    }

    bool found = false;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        if (columnName == columnText(statement, 1)) {
            found = true;
            break;
        }
    }

    sqlite3_finalize(statement);
    return found;
}

bool Storage::execute(const char *sql)
{
    return execute(std::string(sql));
}

bool Storage::execute(const std::string &sql)
{
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return false;
    }

    char *errorMessage = nullptr;
    const int result = sqlite3_exec(m_database, sql.c_str(), nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK) {
        m_lastError = errorMessage != nullptr ? errorMessage : "Unknown SQLite error.";
        sqlite3_free(errorMessage);
        return false;
    }

    m_lastError.clear();
    return true;
}

bool Storage::bindAndStep(sqlite3_stmt *statement)
{
    const int result = sqlite3_step(statement);
    if (result != SQLITE_DONE) {
        setLastSqliteError("SQLite statement failed");
        return false;
    }

    m_lastError.clear();
    return true;
}

void Storage::setLastSqliteError(const std::string &prefix)
{
    if (m_database == nullptr) {
        m_lastError = prefix + ": database handle is not available.";
        return;
    }

    m_lastError = prefix + ": " + sqlite3_errmsg(m_database);
}

} // namespace local_jarvis::storage
