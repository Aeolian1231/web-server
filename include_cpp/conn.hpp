#pragma once
#include "buffer.hpp"
#include "http.hpp"
#include "http_response.hpp"

#include <netinet/in.h>
#include <mutex>
#include <string>

class Conn {
public:
    Conn() = default;
    Conn(int fd, const sockaddr_in& addr);

    int fd() const { return fd_; }

    // Reactor 调用：从 fd 读入 in_，只做读取与返回是否发现完整 header（不解析）
    // 返回: -1 error/close; 0 no complete header yet; >0 完整header长度（可用于提交任务）
    ssize_t readIntoBuffer();

    // 新增：返回当前可读字节数（Reactor 用于判断是否存在 header）
    size_t readableBytes() const;

    // 新增：复制前 len 字节为 std::string（线程安全：Reactor 在主线程调用）
    std::string peekReadableAsString(size_t len) const;

    // 新增：从 in_ 中消费 len 字节（Reactor 主线程调用）
    void retrieve(size_t len);

    // Worker 调用：解析已填充的 in_ 副本并构造响应，然后把响应设置到 Conn（thread-safe）
    void setResponseFromWorker(const std::string& response);

    // Reactor 调用：写 out_，返回 true if all sent and should close (阶段2统一close)
    bool writeOut();

    // 直接关闭 fd
    void closeFd();

    // 不允许拷贝
    Conn(const Conn&) = delete;
    Conn& operator=(const Conn&) = delete;

private:
    int fd_ = -1;
    sockaddr_in addr_{};

    Buffer in_;   // 仅 Reactor 写入（主线程），worker 读取时使用副本（由 Reactor 提供）
    Buffer out_;  // worker 写入 via setResponseFromWorker (protected by mutex)
    mutable std::mutex out_mtx_; // 保护 out_ 与 responseReady_
    bool responseReady_ = false;
};