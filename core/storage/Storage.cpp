#include "Storage.h"

#include <sqlite3.h>

#include <chrono>
#include <cstdint>
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

constexpr const char *kSchemaSql = R"sql(
CREATE TABLE IF NOT EXISTS sessions (
    id TEXT PRIMARY KEY,
    mode TEXT NOT NULL,
    started_at TEXT NOT NULL,
    ended_at TEXT NULL,
    title TEXT NULL
);

CREATE TABLE IF NOT EXISTS transcript_segments (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    start_ms INTEGER NOT NULL,
    end_ms INTEGER NOT NULL,
    speaker TEXT NULL,
    text TEXT NOT NULL,
    source TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS screen_ocr_segments (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    timestamp_ms INTEGER NOT NULL,
    window_title TEXT NULL,
    text TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS processed_notes (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    type TEXT NOT NULL,
    title TEXT NOT NULL,
    body TEXT NOT NULL,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS action_items (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    text TEXT NOT NULL,
    status TEXT NOT NULL,
    due_at TEXT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS flashcards (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    question TEXT NOT NULL,
    answer TEXT NOT NULL,
    topic TEXT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS privacy_events (
    id TEXT PRIMARY KEY,
    session_id TEXT NULL,
    event_type TEXT NOT NULL,
    details TEXT NOT NULL,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_transcript_segments_session_id
    ON transcript_segments(session_id);

CREATE INDEX IF NOT EXISTS idx_screen_ocr_segments_session_id
    ON screen_ocr_segments(session_id);

CREATE INDEX IF NOT EXISTS idx_processed_notes_session_id
    ON processed_notes(session_id);

CREATE INDEX IF NOT EXISTS idx_action_items_session_id
    ON action_items(session_id);

CREATE INDEX IF NOT EXISTS idx_flashcards_session_id
    ON flashcards(session_id);

CREATE INDEX IF NOT EXISTS idx_privacy_events_session_id
    ON privacy_events(session_id);

CREATE INDEX IF NOT EXISTS idx_sessions_started_at
    ON sessions(started_at);
)sql";

std::string pathToUtf8(const std::filesystem::path &path)
{
    const auto value = path.u8string();
    return { reinterpret_cast<const char *>(value.data()), value.size() };
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

} // namespace

Storage::~Storage()
{
    close();
}

std::filesystem::path Storage::defaultDatabasePath()
{
    return std::filesystem::current_path() / "data" / "local_jarvis.db";
}

bool Storage::open()
{
    return open(defaultDatabasePath());
}

bool Storage::open(const std::filesystem::path &databasePath)
{
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
    if (m_database != nullptr) {
        sqlite3_close(m_database);
        m_database = nullptr;
    }
}

bool Storage::createSchema()
{
    return execute(kSchemaSql);
}

bool Storage::initializeSchema()
{
    return createSchema();
}

std::optional<std::string> Storage::createSession(const std::string &mode)
{
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return std::nullopt;
    }

    constexpr const char *sql = "INSERT INTO sessions (id, mode, started_at) VALUES (?, ?, ?);";
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
    bindText(statement, 3, startedAt);

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);
    if (!ok) {
        return std::nullopt;
    }

    return sessionId;
}

bool Storage::endSession(const std::string &sessionId)
{
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

    const std::string endedAt = utcNow();
    bindText(statement, 1, endedAt);
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
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return std::nullopt;
    }

    constexpr const char *sql = R"sql(
INSERT INTO transcript_segments (id, session_id, start_ms, end_ms, speaker, text, source)
VALUES (?, ?, ?, ?, ?, ?, ?);
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

    const bool ok = bindAndStep(statement);
    sqlite3_finalize(statement);
    if (!ok) {
        return std::nullopt;
    }

    return id;
}

std::optional<std::string> Storage::addProcessedNote(const ProcessedNoteInput &note)
{
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return std::nullopt;
    }

    constexpr const char *sql = R"sql(
INSERT INTO processed_notes (id, session_id, type, title, body, created_at)
VALUES (?, ?, ?, ?, ?, ?);
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
    bindText(statement, 4, note.title);
    bindText(statement, 5, note.body);
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
    std::vector<SessionRecord> sessions;

    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return sessions;
    }

    constexpr const char *sql = R"sql(
SELECT id, mode, started_at, ended_at, title
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
            .title = nullableColumnText(statement, 4)
        });
    }

    sqlite3_finalize(statement);
    return sessions;
}

int Storage::countTranscriptSegmentsForSession(const std::string &sessionId)
{
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

bool Storage::isOpen() const
{
    return m_database != nullptr;
}

const std::string &Storage::lastError() const
{
    return m_lastError;
}

bool Storage::execute(const char *sql)
{
    if (!isOpen()) {
        m_lastError = "Database is not open.";
        return false;
    }

    char *errorMessage = nullptr;
    const int result = sqlite3_exec(m_database, sql, nullptr, nullptr, &errorMessage);
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
