/******************************************************************************
* echo_client.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo client.  The  *
*              client connects to an arbitrary <host,port> and sends input    *
*              from stdin.                                                    *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#define ECHO_PORT 9999
#define BUF_SIZE 4096
# define AI_PASSIVE	0x0001

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <server-ip> <port>",argv[0]);
        return EXIT_FAILURE;
    }

    char buf[BUF_SIZE];
    int status, sock;
    struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
    struct addrinfo *servinfo; //will point to the results
    hints.ai_family = AF_INET;  //IPv4
    hints.ai_socktype = SOCK_STREAM; //TCP stream sockets
    hints.ai_flags = AI_PASSIVE; //fill in my IP for me

    if ((status = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) //解析服务器地址和端口
    {
        fprintf(stderr, "getaddrinfo error: %s \n", gai_strerror(status));
        return EXIT_FAILURE;
    }

    if((sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) //创建套接字
    {
        fprintf(stderr, "Socket failed");
        return EXIT_FAILURE;
    }
    
    if (connect (sock, servinfo->ai_addr, servinfo->ai_addrlen) == -1) //建立连接
    {
        fprintf(stderr, "Connect error\n");
        return EXIT_FAILURE;
    }
        
    char msg[BUFSIZ];
    
    while(1) {
        // fgets(msg, BUFSIZ, stdin);

        //start
        fgets(msg, BUFSIZ, stdin);
        size_t len = strlen(msg);
        if (len > 0 && msg[len - 1] == '\n') { //去掉换行符
            msg[len - 1] = '\0';
        }
        const int fd_in = open(msg, 00);
        if (fd_in < 0) {
            printf("=========================== Failed to open the file ============================\n");
            return 0;
        }
        const int readRet = (int) read(fd_in, msg, BUFSIZ);
        //end
        
        //检查空输入
        if(strlen(msg) == 0) 
            return EXIT_SUCCESS;

        //发送数据及错误处理
        fprintf(stdout, "================================ Sending ==============================\n");
        fprintf(stdout, "%s\n", msg);

        if(send(sock, msg, strlen(msg), 0) == -1)
            perror("send failed \n");

        //接收回应及错误处理
        // while(1){
            int bytes_received;
            if((bytes_received = recv(sock, buf, BUF_SIZE-1, 0)) <= 0) {
                perror("recv");
                return EXIT_SUCCESS;
            }
            
            //打印回应
            buf[bytes_received] = '\0';
            fprintf(stdout, "============================= Received ============================\n");
            fprintf(stdout, "%s \n", buf);
            // if(strstr(buf,"END")){ //收到结束标志
            //     break;
            // }
        // }
    }

    freeaddrinfo(servinfo);
    close(sock);    
    return EXIT_SUCCESS;
}
