#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "net_util.h"

//标记是否调用过WinStartup,避免重复初始化
static int g_net_ready = 0;

int net_init(void)
{
    if (g_net_ready) return 0;
    WSADATA wsa;
    //加载Winsock2.2，失败返回-1
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return -1;
    g_net_ready = 1;
    return 0;
}

void net_cleanup(void)
{
    if (!g_net_ready) return;
    WSACleanup();
    g_net_ready = 0;
}

//关闭连接
void net_close(int fd)
{
    if (fd >= 0)
        closesocket((SOCKET)fd);//Linux下用close()
}

//允许端口复用，防止CtrlC以后，下次运行可能会报错端口被占用，还要杀进程
static int set_reuseaddr(SOCKET s)
{
    BOOL yes = TRUE;
    //setsockopt允许刚关掉的端口马上再bind
    return setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
}

int net_listen_tcp(int port)
{
    //创建IPV4 TCP套接字
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return -1;

    set_reuseaddr(s);
    //ipv4地址结构
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    //INADDR_ANY监听本机所有网卡，然后用htonl将IP地址主机字节序转网络字节序
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //htons将端口号从主机字节序转换为网络字节序（大端）
    addr.sin_port = htons((u_short)port);
    //绑定端口然后监听
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR 
        ||listen(s, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(s);
        return -1;
    }
    //返回监听Socket的int句柄
    return (int)s;
}
//服务器端接收连接
int net_accept(int listen_fd)
{
    //accept函数是阻塞等待，直到有客户端发起连接，后面两个NULL表示不要客户端 IP/端口信息
    SOCKET c = accept((SOCKET)listen_fd, NULL, NULL);
    if (c == INVALID_SOCKET) return -1;
    //连接成功，就返回新的scoket，和客户端通信用这个
    return (int)c;
}

//客户端发起连接
int net_connect_tcp(const char *host, int port)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);

    //host为空就默认回环地址
    const char *target = (host && host[0]) ? host : "127.0.0.1";
    if(strcmp(target, "localhost") == 0) {
        //"localhost",转换为 127.0.0.1
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }else{
        //inet_addr()将IPV4点分十进制字符串转换为网络字节序的32位整数
        addr.sin_addr.s_addr = inet_addr(target);
        if(addr.sin_addr.s_addr == INADDR_NONE){
            closesocket(s);
            return -1;
        }
    }
    //connect()发起 TCP 三次握手
    if(connect(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR){
        closesocket(s);
        return -1;
    }
    //连接到了就返回和服务器的连接句柄
    return (int)s;
}

//fd是socket文件描述符（会被强制转换为 SOCKET），line是要发送的数据
int net_send_line(int fd, const char *line)
{
    char buf[4096];
    //n是实际需要的字符数
    int n = snprintf(buf, sizeof(buf), "%s\n", line != NULL ? line : "");
    if (n <= 0 || n >= (int)sizeof(buf)) return -1;

    int sent = 0;
    while (sent < n) {
        //就是这一次的seq = 上一次的seq + 上次发出去的字节数
        int r = send((SOCKET)fd, buf + sent, n - sent, 0);
        if (r == SOCKET_ERROR || r == 0) return -1;
        sent += r;
    }
    return 0;
}

int net_recv_line(int fd, char *buf, int size)
{
    if (!buf || size <= 0) return -1;

    int pos = 0;
    //预留一个位置结尾放'\0'
    while (pos < size - 1) {
        char ch;
        int r = recv((SOCKET)fd, &ch, 1, 0);
        if (r <= 0) return -1;
        if (ch == '\n') break;
        if (ch != '\r'){
            //跳过不存入'\r'
            buf[pos++] = ch;
        }
    }
    buf[pos] = '\0';
    return 0;
}
