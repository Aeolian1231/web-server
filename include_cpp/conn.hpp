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

    // Reactor: read into in-buffer; returns -1 on error/close, 0 incomplete, >0 header length
    ssize_t readIntoBuffer();

    // readable size and safe copy / consume
    size_t readableBytes() const;
    std::string peekReadableAsString(size_t len) const;
    void retrieve(size_t len);

    // Worker sets response
    void setResponseFromWorker(const std::string& response);

    // Reactor writes out the out buffer; return true if finished and should close
    bool writeOut();

    // Close fd
    void closeFd();

    // Peer ip as string
    std::string peerIp() const;

    Conn(const Conn&) = delete;
    Conn& operator=(const Conn&) = delete;

private:
    int fd_ = -1;
    sockaddr_in addr_{};

    Buffer in_;
    Buffer out_;
    mutable std::mutex out_mtx_;
    bool responseReady_ = false;
};