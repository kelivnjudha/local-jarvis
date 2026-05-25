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
};

struct TranscriptSegmentInput {
    std::string sessionId;
    std::int64_t startMs = 0;
    std::int64_t endMs = 0;
    std::optional<std::string> speaker;
    std::string text;
    std::string source;
};

struct ProcessedNoteInput {
    std::string sessionId;
    std::string type;
    std::string title;
    std::string body;
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

    bool open();
    bool open(const std::filesystem::path &databasePath);
    void close();

    bool createSchema();
    bool initializeSchema();

    std::optional<std::string> createSession(const std::string &mode);
    bool endSession(const std::string &sessionId);
    std::optional<std::string> addTranscriptSegment(const TranscriptSegmentInput &segment);
    std::optional<std::string> addProcessedNote(const ProcessedNoteInput &note);
    [[nodiscard]] std::vector<SessionRecord> listRecentSessions(int limit = 10);
    [[nodiscard]] int countTranscriptSegmentsForSession(const std::string &sessionId);

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] const std::string &lastError() const;

private:
    bool execute(const char *sql);
    bool bindAndStep(sqlite3_stmt *statement);
    void setLastSqliteError(const std::string &prefix);

    sqlite3 *m_database = nullptr;
    std::string m_lastError;
};

} // namespace local_jarvis::storage
