#ifndef SQLITE_LOGGER_H
#define SQLITE_LOGGER_H

#include "core/Types.h"
#include <string>
#include <vector>
#include <mutex>

struct sqlite3;

namespace recorder {

class SqliteLogger {
public:
    SqliteLogger();
    ~SqliteLogger();

    bool Initialize(const std::string& dbPath);
    void Close();

    int64_t LogCallStart(const std::string& contactOrNumber, const std::string& filePath);
    bool LogCallEnd(int64_t callId, double durationSeconds);
    std::vector<CallRecord> GetCallHistory() const;

private:
    sqlite3* db_{nullptr};
    mutable std::mutex mutex_;
};

} // namespace recorder

#endif // SQLITE_LOGGER_H
