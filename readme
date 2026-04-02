# web-server

一个使用 **C++17** 编写的轻量级 Web Server，核心采用 **epoll + Reactor** 的事件驱动模型，并配合 **线程池** 处理业务逻辑，实现高并发场景下的连接管理与请求处理。

项目包含连接超时控制（定时器/统一事件源思想）、ET 模式下的事件处理策略、读写缓冲区与基础 HTTP 请求解析/响应构造，同时提供异步日志模块，便于观察服务器运行状态与调试。

## 主要特性

- **epoll Reactor**：主循环基于 `epoll_wait` 统一处理 IO 事件
- **ET(Edge Triggered) 处理**：通过循环 `accept/read` 直到 `EAGAIN`，避免漏处理事件
- **连接超时管理**：使用定时器结构维护连接超时，动态计算 `epoll_wait` 超时时间，避免空转/延迟关闭
- **线程池**：将请求解析/业务处理投递到 worker 线程执行
- **Buffer / HTTP 模块**：维护读写缓冲区，支持按 `\r\n\r\n` 判断 HTTP 头是否接收完整
- **HTTP Response**：构造并返回基础 HTTP 响应
- **异步日志**：降低日志 IO 对主线程的影响

## 快速开始（本地编译运行）

### 依赖
- Linux（使用 epoll）
- g++（支持 C++17）
- pthread

### 编译
```bash
make
```

### 运行
```bash
./bin/webserver_epoll
```

如需清理：
```bash
make clean_epoll
```

## 目录结构（简要）

- `src_cpp/`：核心实现（epoll、连接、HTTP、线程池、定时器、日志等）
- `include_cpp/`：对应头文件
- `static_site/`：静态资源（若作为静态站点根目录可直接放置页面）
- `cgi/`：CGI 相关内容（如果后续扩展动态接口可放这里）
- `logger/`：日志相关模块/输出目录（视实现而定）
- `bin/`：构建输出目录（可执行文件）
- `Makefile`：编译脚本
