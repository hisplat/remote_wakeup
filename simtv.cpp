

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <list>
#include <vector>
#include <map>
#include <string>
#include <sstream>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <arpa/inet.h>


typedef struct {
    char id[32];
    char on;
    char reserved1;
    char reserved2;
    char reserved3;
} AlivePacket;


static long long ntick()
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)(t.tv_sec)*1000000000LL + t.tv_nsec;
}

static void reuseaddr(int fd)
{
    int op = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&op, sizeof(op));
}

static void addfd(int epollfd, int fd)
{
    assert(epollfd >= 0);
    assert(fd >= 0);

    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

std::string uuid()
{
    char buf[37];
    char * p = buf;
    const char * c = "89ab";
    unsigned int seed = (int)((ntick() % INT_MAX) & 0xffffffff);
    srand(seed);
    for (int n = 0; n < 16; n++) {
        int b = rand() % 255;
        switch (n) {
        case 6:
            sprintf(p, "4%x", b % 15);
            break;
        case 8:
            sprintf(p, "%c%x", c[rand() % strlen(c)], b % 15);
            break;
        default:
            sprintf(p, "%02x", b);
            break;
        }
        p += 2;
        switch (n) {
        case 3:
        case 5:
        case 7:
        case 9:
            *p = '-';
            p++;
            break;
        }
    }
    *p = '\0';
    return buf;
}


static std::string remoteip = "127.0.0.1";

static void send_alive(int fd, const std::string u, char on = 0)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(remoteip.c_str());
    addr.sin_port = htons(1081);

    AlivePacket packet;
    snprintf(packet.id, 32, "%s", u.c_str());
    packet.on = on;
    sendto(fd, (const char *)&packet, sizeof(packet), 0, (struct sockaddr *)&addr, sizeof(addr));
}

int main(int argc, char * argv[])
{
    int epollfd = epoll_create(1024);
    int alive_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    assert(epollfd >= 0);
    assert(alive_socket >= 0);

    reuseaddr(alive_socket);

    if (argc > 1) {
        remoteip = argv[1];
    }

    std::string u = uuid();

    addfd(epollfd, alive_socket);
    send_alive(alive_socket, u);

    while (true) {
        struct epoll_event events[20];
        long timeout = 3000;
        int nfds = epoll_wait(epollfd, events, 20, timeout);
        printf("nfds = %d\n", nfds);
        if (nfds == 0) {
            send_alive(alive_socket, u, 0);
        } else if (nfds > 0) {
            printf("Wake up!!\n");
            sleep(5);
            send_alive(alive_socket, u, 1);
            break;
        } else {
            perror("epoll_wait");
            break;
        }
    } // while true

    close(epollfd);
    close(alive_socket);
    return 0;
}

