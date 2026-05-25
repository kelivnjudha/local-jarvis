#pragma once

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace local_jarvis::storage {

struct SessionRecord {
    std::string id;
    std::string mode;
    std::string startedAt;
    std::optional<std::string> endedAt;
    std::optional<std::string> title;
    std::string summaryStatus;
};

struct TranscriptSegmentInput {
    std::string sessionId;
    std::int64_t startMs = 0;
    std::int64_t endMs = 0;
    std::optional<std::string> speaker;
    std::string text;
    std::string source;
};

struct TranscriptSegmentRecord {
    std::string id;
    std::string sessionId;
    std::int64_t startMs = 0;
    std::int64_t endMs = 0;
    std::optional<std::string> speaker;
    std::string text;
    std::string source;
    std::string createdAt;
};

struct ScreenOcrSegmentInput {
    std::string sessionId;
    std::int64_t timestampMs = 0;
    std::optional<std::string> windowTitle;
    std::string text;
};

struct ScreenOcrSegmentRecord {
    std::string id;
    std::string sessionId;
    std::int64_t timestampMs = 0;
    std::optional<std::string> windowTitle;
    std::string text;
    std::string createdAt;
};

struct ProcessedNoteInput {
    std::string sessionId;
    std::string type;
    std::optional<std::string> title;
    std::string body;
    std::optional<std::string> jsonBody;
};

struct ProcessedNoteRecord {
    std::string id;
    std::string sessionId;
    std::string type;
    std::optional<std::string> title;
    std::string body;
    std::optional<std::string> jsonBody;
    std::string createdAt;
};

struct ActionItemInput {
    std::string sessionId;
    std::string text;
    std::string status = "open";
    std::optional<std::string> dueAt;
};

struct ActionItemRecord {
    std::string id;
    std::string sessionId;
    std::string text;
    std::string status;
    std::optional<std::string> dueAt;
    std::string createdAt;
};

struct FlashcardInput {
    std::string sessionId;
    std::string question;
    std::string answer;
    std::optional<std::string> topic;
};

struct FlashcardRecord {
    std::string id;
    std::string sessionId;
    std::string question;
    std::string answer;
    std::optional<std::string> topic;
    std::string createdAt;
};

struct PrivacyEventInput {
    std::optional<std::string> sessionId;
    std::string eventType;
    std::string details;
};

struct SearchResult {
    std::string id;
    std::string sessionId;
    std::string text;
    double rank = 0.0;
};

class Storage {
public:
    Storage() = default;
    ~Storage();

    Storage(const Storage &) = delete;
    Storage &operator=(const Storage &) = delete;
    Storage(Storage &&) = delete;
    Storage &operator=(Storage &&) = delete;

    [[nodiscard]] static std::filesystem::path defaultDatabasePath();

    bool initialize();
    bool initialize(const std::filesystem::path &databasePath);
    bool open();
    bool open(const std::filesystem::path &databasePath);
    void close();

    bool createSchema();
    bool initializeSchema();

    std::optional<std::string> createSession(
        const std::string &mode,
        const std::optional<std::string> &title = std::nullopt);
    bool endSession(const std::string &sessionId);
    std::optional<std::string> addTranscriptSegment(const TranscriptSegmentInput &segment);
    std::optional<std::string> addScreenOcrSegment(const ScreenOcrSegmentInput &segment);
    std::optional<std::string> addProcessedNote(const ProcessedNoteInput &note);
    std::optional<std::string> addActionItem(const ActionItemInput &actionItem);
    std::optional<std::string> addFlashcard(const FlashcardInput &flashcard);
    [[nodiscard]] std::vector<SessionRecord> listRecentSessions(int limit = 10);
    [[nodiscard]] std::vector<TranscriptSegmentRecord> listRecentTranscriptSegments(const std::string &sessionId, int limit = 40);
    [[nodiscard]] std::vector<ScreenOcrSegmentRecord> listRecentScreenOcrSegments(const std::string &sessionId, int limit = 20);
    [[nodiscard]] std::vector<ProcessedNoteRecord> listLatestProcessedNotes(const std::string &sessionId, int limit = 10);
    [[nodiscard]] std::vector<ActionItemRecord> listActionItems(const std::string &sessionId, int limit = 20);
    [[nodiscard]] std::vector<FlashcardRecord> listFlashcards(const std::string &sessionId, int limit = 20);
    [[nodiscard]] int countTranscriptSegmentsForSession(const std::string &sessionId);
    bool setSetting(const std::string &key, const std::string &value);
    [[nodiscard]] std::optional<std::string> getSetting(const std::string &key);
    std::optional<std::string> addModelEvent(
        const std::string &eventType,
        const std::optional<std::string> &modelName,
        const std::optional<std::string> &details);
    std::optional<std::string> addPrivacyEvent(const PrivacyEventInput &event);
    [[nodiscard]] std::vector<SearchResult> searchTranscripts(const std::string &query, int limit = 20);
    [[nodiscard]] std::vector<SearchResult> searchProcessedNotes(const std::string &query, int limit = 20);

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] const std::string &lastError() const;

private:
    bool runMigrations();
    bool ensureCoreSchema();
    bool ensureFtsTables();
    bool isFts5Available();
    bool addColumnIfMissing(const std::string &tableName, const std::string &columnName, const std::string &columnDefinition);
    [[nodiscard]] bool columnExists(const std::string &tableName, const std::string &columnName);
    bool execute(const char *sql);
    bool execute(const std::string &sql);
    bool bindAndStep(sqlite3_stmt *statement);
    void setLastSqliteError(const std::string &prefix);

    sqlite3 *m_database = nullptr;
    bool m_ftsAvailable = false;
    std::string m_lastError;
};

} // namespace local_jarvis::storage
