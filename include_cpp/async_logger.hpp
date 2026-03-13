#pragma once
#include "noncopyable.hpp"
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

class AsyncLogger : private NonCopyable {
public:
    AsyncLogger();
    ~AsyncLogger();

    // Start background writer thread
    void start();

    // Stop background writer thread and flush
    void stop();

    // Log access: host, request_line, status_code, response_size
    void logAccess(const std::string& host, const std::string& request_line, int status_code, long response_size);

    // Log error message
    void logError(const std::string& message);

private:
    void run(); // background loop
    void flushAll();

    std::mutex mtx_;
    std::condition_variable cv_;
    bool stopping_ = false;
    std::thread th_;

    std::queue<std::string> access_q_;
    std::queue<std::string> error_q_;

    std::ofstream access_os_;
    std::ofstream error_os_;
};