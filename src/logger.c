#include "logger.h"
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

static FILE *error_log = NULL;
static FILE *access_log = NULL;

void init_logger() {
    // 打开错误日志文件(追加模式)
    error_log = fopen("./logger/error_log", "a");
    if (!error_log) {
        perror("Failed to open error log file");
        fclose(error_log);
        exit(EXIT_FAILURE);
    }
    
    // 打开访问日志文件(追加模式)
    access_log = fopen("./logger/access_log", "a");
    if (!access_log) {
        perror("Failed to open access log file");
        fclose(access_log);
        exit(EXIT_FAILURE);
    }
}

// void log_wrong(const char *message, const char *client_ip){
//     size_t len = strlen(message)+1;
//     char *mesg = (char *)malloc(len);
//     strcpy(mesg, message);

//     if (mesg == NULL) {
//         return;
//     }
    
//     time_t now;
//     time(&now);
//     // 调整为 UTC+8
//     now += 8 * 60 * 60;
//     struct tm *tm_info = gmtime(&now);

//     char timestamp[32];
//     strftime(timestamp, sizeof(timestamp), "[%a %b %d %H:%M:%S %Y]", tm_info);

//     fprintf(error_log , "%s [error] [%s] %s \n", timestamp, client_ip, mesg);
//     fflush(error_log);

//     // 释放动态分配的内存
//     free(mesg);
// }

void log_error(const char *message, const char* request, const char *client_ip, const int pid, const int tid) {
    // 分配足够的空间来存储拼接后的消息
    size_t len = strlen(message) + strlen(request) + 1;
    char *mesg = (char *)malloc(len);
    strcpy(mesg, message);
    strcat(mesg, request);

    if (mesg == NULL) {
        return;
    }
    
    time_t now;
    time(&now);
    // 调整为 UTC+8
    now += 8 * 60 * 60;
    struct tm *tm_info = gmtime(&now);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%a %b %d %H:%M:%S %Y]", tm_info);

    fprintf(error_log, "%s [error] [pid %d :tid %d] [%s] %s \n", timestamp, pid, tid, client_ip , mesg);
    fflush(error_log);

    // 释放动态分配的内存
    free(mesg);
}

void log_access(const char *client_ip, const char *request_line, 
               int status_code, long response_size) {
    if (!access_log) return;
    
    time_t now;
    time(&now);
    // 调整为 UTC+8
    now += 8 * 60 * 60;
    struct tm *tm_info = gmtime(&now);
    
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%d/%b/%Y:%H:%M:%S %z", tm_info);
    
    // Common Log Format:
    // host ident authuser [date] "request" status bytes
    fprintf(access_log, "%s -- [%s] \"%s\" %d %ld\n",
            client_ip, timestamp, request_line, status_code, response_size);
    fflush(access_log);
}

void close_logger() {
    if (error_log) fclose(error_log);
    if (access_log) fclose(access_log);
    error_log = NULL;
    access_log = NULL;
}