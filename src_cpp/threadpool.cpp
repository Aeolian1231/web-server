#include "threadpool.hpp"
#include <stdexcept>

ThreadPool::ThreadPool(size_t nThreads) {
    if (nThreads == 0) nThreads = 1;
    workers_.reserve(nThreads);
    for (size_t i = 0; i < nThreads; ++i) {
        workers_.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start() {
    // 已经在构造函数创建线程，nothing
}

void ThreadPool::stop() {
    {
        std::unique_lock<std::mutex> lk(mtx_);
        stopping_ = true;
    }
    cv_.notify_all();
    for (auto &t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lk(mtx_);
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [&]{ return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty()) return;
            job = std::move(tasks_.front());
            tasks_.pop();
        }
        try {
            job();
        } catch (...) {
            // ignore exceptions in worker tasks
        }
    }
}