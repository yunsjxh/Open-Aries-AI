#pragma once

// windows.h pollutes these as macros; undefine before use
#ifdef ERROR
#undef ERROR
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef INFO
#undef INFO
#endif
#ifdef WARN
#undef WARN
#endif

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <functional>
#include <vector>
#include <deque>
#include <ctime>

namespace aries {

enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

struct LogEntry {
    uint64_t id = 0;           // auto-increment, unique across session
    std::string timestamp;     // ISO 8601
    LogLevel level = LogLevel::INFO;
    std::string session_id;
    int round_id = -1;
    std::string phase;         // "init", "screenshot", "ai_call", "parse", "execute", "task", etc.
    std::string message;
    int64_t duration_ms = -1;  // -1 = not applicable
    int token_usage = -1;      // -1 = not tracked
    std::string trace_id;      // from API response header
    std::string error_code;    // empty = no error
    std::string extra;         // optional JSON for arbitrary key-value pairs
};

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void init(const std::string& logFile) {
        std::lock_guard<std::mutex> lock(mutex_);
        log_file_ = logFile;
        session_id_ = generateSessionId();

        std::ofstream f(log_file_, std::ios::trunc);
        if (f.is_open()) {
            LogEntry entry;
            entry.timestamp = nowStr();
            entry.level = LogLevel::INFO;
            entry.session_id = session_id_;
            entry.phase = "init";
            entry.message = "session_start";
            writeLocked(entry);
        }
        initialized_ = true;
    }

    void setLogCallback(std::function<void(const LogEntry&)> cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = std::move(cb);
    }

    void setRoundId(int id) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_round_ = id;
    }
    void setTraceId(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_trace_id_ = id;
    }
    void setTokenUsage(int tokens) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_token_usage_ = tokens;
    }

    // ── Core logging methods ──

    void log(LogLevel level, const std::string& message, int round = -1,
             const std::string& phase = "", int64_t durationMs = -1,
             int tokenUsage = -1, const std::string& traceId = "",
             const std::string& errorCode = "", const std::string& extra = "")
    {
        if (!initialized_) return;
        std::lock_guard<std::mutex> lock(mutex_);

        LogEntry entry;
        entry.timestamp = nowStr();
        entry.level = level;
        entry.session_id = session_id_;
        entry.round_id = round >= 0 ? round : current_round_;
        entry.phase = phase;
        entry.message = message;
        entry.duration_ms = durationMs;
        entry.token_usage = tokenUsage >= 0 ? tokenUsage : current_token_usage_;
        entry.trace_id = traceId.empty() ? current_trace_id_ : traceId;
        entry.error_code = errorCode;
        entry.id = next_log_id_++;

        if (!extra.empty() && extra.front() != '{') {
            entry.extra = "{\"value\":\"" + extra + "\"}";
        } else {
            entry.extra = extra;
        }

        writeLocked(entry);
        ring_buffer_.push_back(entry);
        while (ring_buffer_.size() > MAX_RING) ring_buffer_.pop_front();

        std::cout << formatConsole(entry) << std::endl;

        if (callback_) callback_(entry);

        // Reset per-call state
        current_trace_id_.clear();
        current_token_usage_ = -1;
    }

    void debug(const std::string& msg, int round = -1, const std::string& phase = "") {
        log(LogLevel::DEBUG, msg, round, phase);
    }
    void info(const std::string& msg, int round = -1, const std::string& phase = "") {
        log(LogLevel::INFO, msg, round, phase);
    }
    void warn(const std::string& msg, int round = -1, const std::string& phase = "") {
        log(LogLevel::WARN, msg, round, phase);
    }
    void error(const std::string& msg, int round = -1, const std::string& phase = "",
               const std::string& errCode = "") {
        log(LogLevel::ERROR, msg, round, phase, -1, -1, "", errCode);
    }

    // ── Timed operation ──

    void infoWithDuration(const std::string& msg, int64_t durationMs,
                          int round = -1, const std::string& phase = "") {
        log(LogLevel::INFO, msg, round, phase, durationMs);
    }
    void errorWithDuration(const std::string& msg, int64_t durationMs,
                           int round = -1, const std::string& phase = "",
                           const std::string& errCode = "") {
        log(LogLevel::ERROR, msg, round, phase, durationMs, -1, "", errCode);
    }

    // ── Ring buffer access (for Web GUI) ──

    std::vector<LogEntry> getRecentLogs(size_t count = 100) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<LogEntry> result;
        size_t start = ring_buffer_.size() > count ? ring_buffer_.size() - count : 0;
        for (size_t i = start; i < ring_buffer_.size(); ++i)
            result.push_back(ring_buffer_[i]);
        return result;
    }

    std::string getRecentLogsJson(size_t count = 100) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << "{\"session_id\":\"" << escapeJson(session_id_) << "\",\"logs\":[";
        size_t start = ring_buffer_.size() > count ? ring_buffer_.size() - count : 0;
        for (size_t i = start; i < ring_buffer_.size(); ++i) {
            if (i > start) oss << ",";
            oss << entryToJson(ring_buffer_[i]);
        }
        oss << "]}";
        return oss.str();
    }

    // ── Properties ──

    const std::string& sessionId() const { return session_id_; }
    int currentRound() const { return current_round_; }

    // ── Scoped timer ──

    class ScopedTimer {
    public:
        explicit ScopedTimer(const std::string& operation)
            : operation_(operation), start_(std::chrono::steady_clock::now()) {}

        ~ScopedTimer() {
            auto end = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
            Logger::instance().infoWithDuration(operation_ + " 完成", ms, -1, operation_);
        }

        int64_t elapsedMs() const {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
        }

        // Move-only
        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;
        ScopedTimer(ScopedTimer&&) = default;
        ScopedTimer& operator=(ScopedTimer&&) = default;

    private:
        std::string operation_;
        std::chrono::steady_clock::time_point start_;
    };

private:
    Logger() = default;

    bool initialized_ = false;
    std::string session_id_;
    std::string log_file_;
    mutable std::mutex mutex_;
    int current_round_ = -1;
    std::string current_trace_id_;
    int current_token_usage_ = -1;
    std::deque<LogEntry> ring_buffer_;
    static constexpr size_t MAX_RING = 2000;
    std::function<void(const LogEntry&)> callback_;
    uint64_t next_log_id_ = 1;

    std::string generateSessionId() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        std::ostringstream oss;
        oss << std::hex << (ms & 0xFFFFFFFF) << "-" << dis(gen);
        return oss.str();
    }

    std::string nowStr() {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        std::tm tm;
        localtime_s(&tm, &t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    void writeLocked(const LogEntry& entry) {
        if (log_file_.empty()) return;
        std::ofstream f(log_file_, std::ios::app);
        if (f.is_open()) {
            f << entryToJson(entry) << std::endl;
        }
    }

    static std::string entryToJson(const LogEntry& e) {
        std::ostringstream oss;
        oss << "{"
            << "\"id\":" << e.id
            << ",\"ts\":\"" << e.timestamp << "\""
            << ",\"level\":\"" << levelStr(e.level) << "\""
            << ",\"sid\":\"" << e.session_id << "\""
            << ",\"round\":" << e.round_id
            << ",\"phase\":\"" << escapeJson(e.phase) << "\""
            << ",\"msg\":\"" << escapeJson(e.message) << "\"";
        if (e.duration_ms >= 0)
            oss << ",\"dur_ms\":" << e.duration_ms;
        if (e.token_usage >= 0)
            oss << ",\"tokens\":" << e.token_usage;
        if (!e.trace_id.empty())
            oss << ",\"trace_id\":\"" << escapeJson(e.trace_id) << "\"";
        if (!e.error_code.empty())
            oss << ",\"err\":\"" << escapeJson(e.error_code) << "\"";
        if (!e.extra.empty())
            oss << ",\"extra\":" << e.extra;
        oss << "}";
        return oss.str();
    }

    static std::string formatConsole(const LogEntry& e) {
        const char* lv = "?";
        switch (e.level) {
            case LogLevel::DEBUG: lv = "DEBUG"; break;
            case LogLevel::INFO:  lv = "INFO "; break;
            case LogLevel::WARN:  lv = "WARN "; break;
            case LogLevel::ERROR: lv = "ERROR"; break;
        }
        std::ostringstream oss;
        oss << "[" << e.timestamp.substr(11, 12) << " " << lv << "]";
        if (e.round_id >= 0) oss << " [R" << e.round_id << "]";
        if (!e.phase.empty()) oss << " [" << e.phase << "]";
        if (e.duration_ms >= 0) oss << " (" << e.duration_ms << "ms)";
        if (e.token_usage >= 0) oss << " (tokens:" << e.token_usage << ")";
        oss << " " << e.message;
        if (!e.error_code.empty()) oss << " [err:" << e.error_code << "]";
        return oss.str();
    }

    static std::string levelStr(LogLevel lv) {
        switch (lv) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO";
            case LogLevel::WARN:  return "WARN";
            case LogLevel::ERROR: return "ERROR";
        }
        return "INFO";
    }

    static std::string escapeJson(const std::string& s) {
        std::string r;
        r.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '"':  r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n";  break;
                case '\r': r += "\\r";  break;
                case '\t': r += "\\t";  break;
                default:   r += c;
            }
        }
        return r;
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

} // namespace aries
