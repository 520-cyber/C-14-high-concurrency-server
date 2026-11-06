#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>

// 引入线程池
#include "ThreadPool.h"

#define MAX_EVENTS 1024
#define PORT 8888

// 将 fd 设为非阻塞
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    //初始化 Socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 128);
    set_nonblocking(server_fd);

    //初始化 Epoll
    int epfd = epoll_create(1);
    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    std::cout << "高并发服务器启动！端口: " << PORT << std::endl;

    //雇佣线程池
    ThreadPool pool(8);

    //核心 Reactor 循环
    while (true) {
        // 主线程死等事件发生
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        
        for (int i = 0; i < nfds; ++i) {
            int active_fd = events[i].data.fd;

            //新客户端连接进来了
            if (active_fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                
                if (client_fd != -1) {
                    set_nonblocking(client_fd);
                    std::cout << "[主线程] 新客户端连接！FD: " << client_fd << std::endl;
                    
                    // 把新客户端的 fd 注册进 epoll
                    ev.events = EPOLLIN; 
                    ev.data.fd = client_fd;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
                }
            } 
            //某个客户端发数据了！
            else {
                //主线程不负责读数据，直接把任务扔进线程池！
                // 注意：Lambda 捕获了 active_fd、epfd 的指针。
                pool.enqueue([active_fd, epfd]() {
                    char buffer[1024] = {0};
                    int bytes_read = recv(active_fd, buffer, sizeof(buffer) - 1, 0);
                    
                    if (bytes_read > 0) {
                        std::cout << "[子线程 " << std::this_thread::get_id() << "] 处理 FD: " 
                                  << active_fd << " 数据: " << buffer;
                        // 业务处理：这里可以写解析 HTTP 的逻辑
                        send(active_fd, buffer, bytes_read, 0); // 先做个 Echo
                    } 
                    else if (bytes_read == 0) {
                        // 客户端断开
                        std::cout << "[子线程] 客户端 FD: " << active_fd << " 断开。" << std::endl;
                        close(active_fd);
                        // 从 epoll 中移除
                        epoll_ctl(epfd, EPOLL_CTL_DEL, active_fd, nullptr);
                    }
                });
            }
        }
    }
    return 0;
}