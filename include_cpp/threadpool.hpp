#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t nThreads);
    ~ThreadPool();

    void start();
    void stop();

    // 提交任务（可捕获 conn fd 和 docroot）
    void submit(std::function<void()> task);

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stopping_ = false;
};