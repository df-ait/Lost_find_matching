#ifndef NET_UTIL_H
#define NET_UTIL_H
//初始化Winsock
int net_init(void);
//释放Winsock
void net_cleanup(void);
//服务器端--(管理员这一端)监听
int net_listen_tcp(int port);
//服务器端--接受客户端连接
int net_accept(int listen_fd);

//客户端--连接服务器
int net_connect_tcp(const char *host, int port);

//both关闭连接
void net_close(int fd);
//发送/读取一行文本
int net_send_line(int fd, const char *line);
int net_recv_line(int fd, char *buf, int size);

#endif /* NET_UTIL_H */
