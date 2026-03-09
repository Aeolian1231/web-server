/*
 *                    _ooOoo_
 *                   o8888888o
 *                   88" . "88
 *                   (| -_- |)
 *                    O\ = /O
 *                ____/`---'\____
 *              .   ' \\| |// `.
 *               / \\||| : |||// \
 *             / _||||| -:- |||||- \
 *               | | \\\ - /// | |
 *             | \_| ''\---/'' | |
 *              \ .-\__ `-` ___/-. /
 *           ___`. .' /--.--\ `. . __
 *        ."" '< `.___\_<|>_/___.' >'"".
 *       | | : `- \`.;`\ _ /`;.`/ - ` : | |
 *         \ \ `-. \_ __\ /__ _/ .-` / /
 * ======`-.____`-.___\_____/___.-`____.-'======
 *                    `=---='
 *
 * .................................................
 *          佛祖保佑             永无BUG
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include "/usr/include/asm-generic/socket.h"
#include <parse.h>
#include "logger.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/select.h>
#include <sys/time.h>
// #include <struct_timeval.h>
#define ECHO_PORT 9999
#define BUF_SIZE 40000

struct sockaddr_in cli_addr;
int sock = -1, client_sock = -1;
char buf[BUF_SIZE];
int maxfd;
fd_set rdset, tmp;// 创建一个fd_set的集合，存放的是需要检测的文件描述符
struct timeval timeout;
int cli_num;
int cnt = 0;

struct ClientInfo {
    struct sockaddr_in addr;
    char ip[INET_ADDRSTRLEN];
    time_t last_active;
} clients[1024];

int close_socket(int sock) {
    if (close(sock)) {
        //fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    close_logger();
    return 0;
}

void handle_signal(const int sig) {
    for (int i = sock + 1; i <= maxfd; i++) {
        if (FD_ISSET(i, &rdset)) {
            close(i);
        }
    }
    close(sock);
    exit(0);
}

void handle_sigpipe(const int sig) {
    if (sock != -1) {
        return;
    }
    exit(0);
}

int max(int a, int b){
    return a>b ? a:b;
}

int Send_back(int client_sock_i){
    pid_t pid = getpid();
    int tid = syscall(SYS_gettid);

    int cnt = 0;
    char* nextRequest = buf;
    int res = 1;
    while(1){ // 循环分割请求
        cnt++;
        char* return_buf = (char* )malloc(20000);
        if (return_buf == NULL) {// 检查分配是否成功
            perror("malloc");
            res = -1;
            return res;
        }
        if(cnt != 1){
            if((nextRequest = strstr(nextRequest,"\r\n\r\n")) == NULL){ // 若找到\r\n\r\n,指针指向首次出现的位置
                //fprintf(stdout,"END\n");
                // res = -1;
                return res;
                // break;
            }
            nextRequest += 4; // 跳过\r\n\r\n
            if(*nextRequest == 0){
                // res = -1;
                return res;
                // break;
            }
            while(*nextRequest == '\r' || *nextRequest == '\n'){ // 跳过所有\r\n
                nextRequest+=1;
                if(*nextRequest == 0){
                    // char* end_ret = "END\r\n\r\n";
                    // res = send(client_sock_i, end_ret, strlen(end_ret), 0);
                    // res = -1;
                    return res;
                    // break;
                }
            }
        }
        // //fprintf(stdout,"||||||||||||||||||||||||||| parse OK |||||||||||||||||||||||||||||\n");
        Request *request = parse(nextRequest, strlen(nextRequest), client_sock_i); // 解析http请求

        // 记录访问日志
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clients[client_sock_i].addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        if (request == NULL) {//格式错误
            strcpy(return_buf, "HTTP/1.1 400 Bad request\r\n\r\n");
            log_error("Wrong HTTP request: ", "NULL", client_ip, pid, tid);
            // return 
                res = send(client_sock_i, return_buf, strlen(return_buf), 0);
        }
        else{
            // 构造请求行
            char request_line[8192];
            // 功能未实现
            snprintf(request_line, sizeof(request_line), "%s %s %s", request->http_method, request->http_uri, request->http_version);
            if(strcmp("HEAD",request->http_method) != 0 && strcmp("GET",request->http_method) != 0 && strcmp("POST",request->http_method) != 0){
                strcpy(return_buf, "HTTP/1.1 501 Not Implemented\r\n\r\n");
                log_error("File does not exit: ", request->http_method, client_ip, pid, tid);
                // return
                    res = send(client_sock_i, return_buf, strlen(return_buf), 0);
            }
            
            else if((strcmp("HTTP/1.1",request->http_version)!=0)){ //版本不正确
                strcpy(return_buf, "HTTP/1.1 505 HTTP Version not supported\r\n\r\n");
                log_error("HTTP Version not supported: ", request->http_version, client_ip, pid, tid);
                // return 
                    res = send(client_sock_i, return_buf, strlen(return_buf), 0);
            }

            else if((strcmp("POST",request->http_method) == 0)){
                // return 
                    snprintf(return_buf, 20000, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
                    res = send(client_sock_i, return_buf, strlen(return_buf), 0);
            }

            else if((strcmp("GET",request->http_method) == 0)){
                char pt[50] = "./static_site"; // 不能用char* 
                if(strcmp(request->http_uri,"/") == 0){
                    strcpy(pt, "./static_site/index.html");
                }
                else{
                    if (strcat(pt, request->http_uri) == NULL) {
                        //fprintf(stderr, "Error strcat.\n");
                        res = -1;
                        return res;
                    }
                }

                FILE *file;
                if(strstr(pt, "html") || strstr(pt, "css")){
                    file = fopen(pt, "r");
                }
                else if(strstr(pt, "png")){
                    file = fopen(pt, "rb");
                }
                
                if(!file){ //文件不存在
                    const char *not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
                    log_error("File does not exit: ", pt, client_ip, pid, tid);
                    // return 
                        res = send(client_sock_i, not_found, strlen(not_found), 0);
                }
                else{
                    // 获取文件大小
                    fseek(file, 0, SEEK_END);
                    long file_size = ftell(file);
                    fseek(file, 0, SEEK_SET);
                    // 读取文件内容
                    char *html_content = malloc(file_size + 1);
                    fread(html_content, 1, file_size, file);
                    fclose(file);
                    // html_content[file_size] = '\0';
                    // 构造响应头
                    char response_head[1024];
                    char *response;
                    int len = 0;
                    if(strstr(pt, "html")){
                        len = snprintf(response_head, sizeof(response_head),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: %ld\r\n"
                            "Connection: close\r\n\r\n",
                            file_size);
                        //连接
                        response = malloc(len + file_size + 1);
                        if (strcpy(response, response_head) == NULL) {
                            //fprintf(stderr, "Error strcpy.\n");
                        }
                        if (memcpy(response+len, html_content, file_size+1) == NULL) {
                            //fprintf(stderr, "Error memcpy.\n");
                        }
                    }
                    else if(strstr(pt, "css")){
                        len = snprintf(response_head, sizeof(response_head),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/css\r\n"
                            "Content-Length: %ld\r\n"
                            "Connection: close\r\n\r\n",
                            file_size);
                        //连接
                        response = malloc(len + file_size + 1);
                        if (strcpy(response, response_head) == NULL) {
                            //fprintf(stderr, "Error strcpy.\n");
                        }
                        if (memcpy(response+len, html_content, file_size+1) == NULL) {
                            //fprintf(stderr, "Error memcpy.\n");
                        }
                    }
                    else if(strstr(pt, "png")){
                        len = snprintf(response_head, sizeof(response_head),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: image/png\r\n"
                            "Content-Length: %ld\r\n"
                            "Connection: close\r\n\r\n",
                            file_size);
                        //连接
                        response = malloc(len + file_size);
                        if (strcpy(response, response_head) == NULL) {
                            //fprintf(stderr, "Error strcpy.\n");
                        }
                        if (memcpy(response+len, html_content, file_size) == NULL) {
                            //fprintf(stderr, "Error memcpy.\n");
                        }
                    }
                    
                    log_access(client_ip, request_line, 200, file_size);

                    // 发送响应
                    res = send(client_sock_i, response, len + file_size, 0);
                    free(html_content);
                    free(response);
                    // return ret;
                }
            }

            else if((strcmp("HEAD",request->http_method) == 0)){
                char pt[50] = "./static_site"; // 不能用char* 
                if(strcmp(request->http_uri,"/") == 0){
                    strcpy(pt, "./static_site/index.html");
                }
                else{
                    if (strcat(pt, request->http_uri) == NULL) {
                        //fprintf(stderr, "Error strcat.\n");
                        res = -1;
                        return res;
                    }
                }
                FILE *file;
                if(strstr(pt, "html") || strstr(pt, "css")){
                    file = fopen(pt, "r");
                }
                else if(strstr(pt, "png")){
                    file = fopen(pt, "rb");
                }
                if(!file){ //文件不存在
                    const char *not_found = 
                        "HTTP/1.1 404 Not Found\r\n\r\n";
                    log_error("File does not exit: ", request->http_uri, client_ip, pid, tid);
                    // return 
                        res = send(client_sock_i, not_found, strlen(not_found), 0);
                }
                else{
                    // 获取文件大小
                    fseek(file, 0, SEEK_END);
                    long file_size = ftell(file);
                    fseek(file, 0, SEEK_SET);

                    // 构造响应头
                    char response_header[512];
                    if(strstr(pt, "html")){
                        snprintf(response_header, sizeof(response_header),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: %ld\r\n"
                        "Connection: close\r\n\r\n",
                        file_size);
                    }
                    if(strstr(pt, "css")){
                        snprintf(response_header, sizeof(response_header),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/css\r\n"
                        "Content-Length: %ld\r\n"
                        "Connection: close\r\n\r\n",
                        file_size);
                    }
                    else if(strstr(pt, "png")){
                        snprintf(response_header, sizeof(response_header),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: image/png\r\n"
                        "Content-Length: %ld\r\n"
                        "Connection: close\r\n\r\n",
                        file_size);
                    }
                    
                    log_access(client_ip, request_line, 200, file_size);
                    // 发送响应
                    // return
                        res = send(client_sock_i, response_header, strlen(response_header), 0);// 响应头
                }
            }

            if(res < 0){
                // free(return_buf);
                return res;
            }
            free(return_buf);
        }
    }
    free(nextRequest);
    return res;
}

int main(int argc, char *argv[]) {
    init_logger();
    cli_num = 0;

    /* register signal handler */
    /* process termination signals */
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGSEGV, handle_signal);//11
    signal(SIGABRT, handle_signal);
    signal(SIGQUIT, handle_signal);
    signal(SIGTSTP, handle_signal);
    signal(SIGFPE, handle_signal);
    signal(SIGHUP, handle_signal);
    /* normal I/O event */
    signal(SIGPIPE, handle_sigpipe);
    socklen_t cli_size;
    struct sockaddr_in addr;
    //fprintf(stdout, "----- Echo Server -----\n");

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        //fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }
    /* set socket SO_REUSEADDR | SO_REUSEPORT */
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        //fprintf(stderr, "Failed setting socket options.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET; // ipv4
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
        close_socket(sock);
        //fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, 5)) {
        close_socket(sock);
        //fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    /* finally, loop waiting for input and then write it back */

    FD_ZERO(&rdset); // 存储所有需要监控的描述符
    FD_SET(sock, &rdset); // 将监听套接字加入集合
    maxfd = sock;
    
    //fprintf(stdout,"============== Waiting for connection... ==============\n");

    while (1) {
        /* listen for new connection */
        tmp = rdset;
        struct timeval *timeout_ptr = NULL;
        if(cli_num > 0){ // 有连接设置5s阻塞
            static struct timeval timeout = {5, 0};  // 5秒超时
            timeout_ptr = &timeout;
        }
        else
            timeout_ptr = NULL;  // 无连接无限期阻塞
        int ret = select(maxfd + 1, &tmp, NULL, NULL, timeout_ptr); // 阻塞等待，ret返回就绪描述符的个数
        
        // 调用select后，tmp只包含就绪的描述符
        if(ret == -1) {
            perror("select");
            exit(-1);
        }
        else if(ret > 0){ // 有描述符就绪
            if(FD_ISSET(sock, &tmp)) { // 监听套接字就绪（有新的连接）
                cli_size = sizeof(cli_addr);
                client_sock = accept(sock, (struct sockaddr *) &cli_addr, &cli_size);
                clients[client_sock].addr = cli_addr;
                // 记录客户端活跃时间
                clients[client_sock].last_active = time(NULL);
                // struct tm *tm_info = localtime(&clients[client_sock].last_active);
                // char time_str[26];  // 足够容纳格式化后的时间字符串
                // strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
                // //fprintf(stdout,"%s\n",time_str);
                inet_ntop(AF_INET, &cli_addr.sin_addr, clients[client_sock].ip, INET_ADDRSTRLEN);
                if (client_sock == -1)
                {
                    //fprintf(stderr, "Error accepting connection.\n");
                    close_socket(sock);
                    return EXIT_FAILURE;
                }
                FD_SET(client_sock, &rdset); // 将新客户端加入监控集合
                maxfd = max(maxfd, client_sock);
                //fprintf(stdout,"============== New connection from %s:%d ==============\n",inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port));
                cli_num++;
                cnt++;
                fprintf(stdout, "%d\n", cnt);
            }
            for(int i = 0; i <= maxfd; i++){
                if(i == sock)
                    continue;
                if(FD_ISSET(i, &tmp)) { //检查客户端套接字是否就绪
                    memset(buf, 0, BUF_SIZE);
                    int readret = recv(i, buf, BUF_SIZE, 0);
                    if(readret <= 0){ // 关闭连接及错误处理
                        // if(readret < 0)
                        //     //fprintf(stdout,"============== Error read from %s:%d ==============\n",clients[i].ip, ntohs(clients[i].addr.sin_port));
                        // else if(readret == 0)
                        //     //fprintf(stdout,"============== Closed connection from %s:%d ==============\n",clients[i].ip, ntohs(clients[i].addr.sin_port));
                        if (close_socket(i)){
                            //fprintf(stderr, "============== Error closing client %s:%d ==============\n",clients[i].ip, ntohs(clients[i].addr.sin_port));
                            return EXIT_FAILURE;
                        }
                        FD_CLR(i, &rdset);
                        cli_num--;
                        if (i == maxfd) {
                            while (maxfd > sock && !FD_ISSET(maxfd, &rdset)) {
                                maxfd--;
                            }
                        }
                    }
                    else{
                        //fprintf(stdout,"============== Received (total %d bytes) from %s:%d ==============\n", readret, clients[i].ip, ntohs(clients[i].addr.sin_port));
                        //fprintf(stdout,"%s\n",buf);
                        clients[i].last_active = time(NULL);
                        if(Send_back(i) < 0){ //传回响应
                            //fprintf(stdout,"============== Error send back to %s ==============", clients[i].ip);
                            if (close_socket(i)){
                                //fprintf(stderr, "============== Error closing client %s ==============\n", clients[i].ip);
                                return EXIT_FAILURE;
                            }
                            FD_CLR(i, &rdset);
                            cli_num--;
                            if (i == maxfd) {
                                while (maxfd > sock && !FD_ISSET(maxfd, &rdset)) {
                                    maxfd--;
                                }
                            }
                        }
                        //fprintf(stdout,"============== Send back to %s:%d ==============\n", clients[i].ip, ntohs(clients[i].addr.sin_port));
                    }
                }
            }
        }
        time_t now = time(NULL);
        for (int i = 0; i <= maxfd; i++){ // 遍历所有客户端，检查是否超时
            if(i == sock)
                continue;
            if (FD_ISSET(i, &rdset)){
                double idle_time = difftime(now, clients[i].last_active);
                // //fprintf(stdout,"%f\n", idle_time);
                if (idle_time >= 5) {  // 超过5秒空闲
                    //fprintf(stdout, "============== Client %s idle for 5 seconds, closing ==============\n", clients[i].ip);
                    close(i);
                    FD_CLR(i, &rdset);
                    cli_num--;
                    if (i == maxfd) {
                        while (maxfd > sock && !FD_ISSET(maxfd, &rdset)) {
                            maxfd--;
                        }
                    }
                }
            }
        }
    }
    close_socket(sock);
    return EXIT_SUCCESS;
}
