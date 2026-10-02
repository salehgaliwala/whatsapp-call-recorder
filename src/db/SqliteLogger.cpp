#include "db/SqliteLogger.h"
#include "sqlite3.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace recorder {

static std::string GetCurrentTimestampISO() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

SqliteLogger::SqliteLogger() = default;

SqliteLogger::~SqliteLogger() {
    Close();
}

bool SqliteLogger::Initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) return true;

    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    const char* createTableSql =
        "CREATE TABLE IF NOT EXISTS call_logs ("
        "call_id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "contact_or_number TEXT NOT NULL, "
        "start_time TEXT NOT NULL, "
        "end_time TEXT, "
        "duration_seconds REAL DEFAULT 0.0, "
        "file_path TEXT NOT NULL);";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, createTableSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    return true;
}

void SqliteLogger::Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

int64_t SqliteLogger::LogCallStart(const std::string& contactOrNumber, const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return -1;

    std::string startTime = GetCurrentTimestampISO();
    const char* insertSql =
        "INSERT INTO call_logs (contact_or_number, start_time, file_path) VALUES (?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, contactOrNumber.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, startTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, filePath.c_str(), -1, SQLITE_TRANSIENT);

    int64_t newId = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        newId = sqlite3_last_insert_rowid(db_);
    }

    sqlite3_finalize(stmt);
    return newId;
}

bool SqliteLogger::LogCallEnd(int64_t callId, double durationSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || callId < 0) return false;

    std::string endTime = GetCurrentTimestampISO();
    const char* updateSql =
        "UPDATE call_logs SET end_time = ?, duration_seconds = ? WHERE call_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, updateSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, endTime.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, durationSeconds);
    sqlite3_bind_int64(stmt, 3, callId);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::vector<CallRecord> SqliteLogger::GetCallHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CallRecord> history;
    if (!db_) return history;

    const char* selectSql =
        "SELECT call_id, contact_or_number, start_time, COALESCE(end_time, ''), duration_seconds, file_path FROM call_logs ORDER BY call_id DESC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, selectSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return history;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CallRecord record;
        record.callId = sqlite3_column_int64(stmt, 0);
        const unsigned char* contact = sqlite3_column_text(stmt, 1);
        record.contactOrNumber = contact ? reinterpret_cast<const char*>(contact) : "";
        const unsigned char* start = sqlite3_column_text(stmt, 2);
        record.startTime = start ? reinterpret_cast<const char*>(start) : "";
        const unsigned char* end = sqlite3_column_text(stmt, 3);
        record.endTime = end ? reinterpret_cast<const char*>(end) : "";
        record.durationSeconds = sqlite3_column_double(stmt, 4);
        const unsigned char* path = sqlite3_column_text(stmt, 5);
        record.filePath = path ? reinterpret_cast<const char*>(path) : "";

        history.push_back(record);
    }

    sqlite3_finalize(stmt);
    return history;
}

} // namespace recorder
