#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#define EXIT_FAILURE 1

// 日志级别
typedef enum {
    LOG_ERROR,
    LOG_ACCESS
} LogLevel;

// 初始化日志系统
void init_logger();

// 记录错误日志
void log_error(const char *message, const char* request, const char *client_ip, const int pid, const int tid);

// 记录访问日志(Common Log Format)
void log_access(const char *client_ip, const char *request_line, 
                int status_code, long response_size);

// 关闭日志系统
void close_logger();

#endif