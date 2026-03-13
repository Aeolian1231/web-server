#include "async_logger.hpp"
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

AsyncLogger::AsyncLogger() {}
AsyncLogger::~AsyncLogger() {
    stop();
}

void AsyncLogger::start() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (th_.joinable()) return;
    // ensure ./logger directory exists
    std::error_code ec;
    std::filesystem::create_directories("./logger", ec);
    access_os_.open("./logger/access_log", std::ios::app | std::ios::out);
    error_os_.open("./logger/error_log", std::ios::app | std::ios::out);
    stopping_ = false;
    th_ = std::thread(&AsyncLogger::run, this);
}

void AsyncLogger::stop() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!th_.joinable()) return;
        stopping_ = true;
    }
    cv_.notify_one();
    if (th_.joinable()) th_.join();
    // flush and close files
    if (access_os_.is_open()) { access_os_.flush(); access_os_.close(); }
    if (error_os_.is_open()) { error_os_.flush(); error_os_.close(); }
}

void AsyncLogger::logAccess(const std::string& host, const std::string& request_line, int status_code, long response_size) {
    // Common Log Format-like: host [timestamp] "request" status bytes
    time_t now = time(nullptr);
    struct tm tm_buf;
    gmtime_r(&now, &tm_buf);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%d/%b/%Y:%H:%M:%S %z", &tm_buf);

    std::ostringstream oss;
    oss << host << " -- [" << timebuf << "] \"" << request_line << "\" " << status_code << " " << response_size << "\n";

    {
        std::lock_guard<std::mutex> lk(mtx_);
        access_q_.push(oss.str());
    }
    cv_.notify_one();
}

void AsyncLogger::logError(const std::string& message) {
    time_t now = time(nullptr);
    struct tm tm_buf;
    gmtime_r(&now, &tm_buf);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_buf);

    std::ostringstream oss;
    oss << "[" << timebuf << "] " << message << "\n";

    {
        std::lock_guard<std::mutex> lk(mtx_);
        error_q_.push(oss.str());
    }
    cv_.notify_one();
}

void AsyncLogger::run() {
    while (true) {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait_for(lk, std::chrono::seconds(1), [&]{ return stopping_ || !access_q_.empty() || !error_q_.empty(); });
        if (!access_q_.empty()) {
            // drain access queue
            while (!access_q_.empty()) {
                const std::string& s = access_q_.front();
                if (access_os_.is_open()) access_os_ << s;
                access_q_.pop();
            }
            if (access_os_.is_open()) access_os_.flush();
        }
        if (!error_q_.empty()) {
            while (!error_q_.empty()) {
                const std::string& s = error_q_.front();
                if (error_os_.is_open()) error_os_ << s;
                error_q_.pop();
            }
            if (error_os_.is_open()) error_os_.flush();
        }
        if (stopping_) break;
    }
    // flush once more
    flushAll();
}

void AsyncLogger::flushAll() {
    if (access_os_.is_open()) { access_os_.flush(); }
    if (error_os_.is_open()) { error_os_.flush(); }
}