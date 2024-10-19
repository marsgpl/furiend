#ifndef EPOLL_STUB_H
#define EPOLL_STUB_H

#include <stdint.h>
#include <sys/socket.h>

#define SOCK_NONBLOCK 0

#define EPOLLIN 0
#define EPOLLPRI 0
#define EPOLLOUT 0
#define EPOLLRDNORM 0
#define EPOLLNVAL 0
#define EPOLLRDBAND 0
#define EPOLLWRNORM 0
#define EPOLLWRBAND 0
#define EPOLLMSG 0
#define EPOLLERR 0
#define EPOLLHUP 0
#define EPOLLRDHUP 0
#define EPOLLEXCLUSIVE 0
#define EPOLLWAKEUP 0
#define EPOLLONESHOT 0
#define EPOLLET 0

#define EPOLL_CTL_ADD 0
#define EPOLL_CTL_DEL 0
#define EPOLL_CTL_MOD 0

typedef union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

struct epoll_event {
    uint32_t events;
    epoll_data_t data;
};

int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int tmt_ms);

int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);

void *memrchr(const void *s, int c, size_t n);

#endif
