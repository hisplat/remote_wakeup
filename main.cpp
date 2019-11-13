

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

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

#include "http_parser.h"

typedef struct {
    char id[32];
    char on;
    char reserved1;
    char reserved2;
    char reserved3;
} AlivePacket;

class AliveClient {
public:
    std::string id;
    std::string status;
    int alive;
    struct sockaddr_in addr;
};

class HttpClient {
public:
    http_parser * parser;
    std::string url;
    bool complete;
};

static const int kMaxAliveTime = 2 * 60 * 1000;

static std::map<std::string, AliveClient> gAliveClients;
static std::map<int, HttpClient *> gHttpClients;
static int alive_socket = -1;


static int now()
{
    struct timespec ts;
    ts.tv_sec = ts.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int n = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    return n;
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

static void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
}

static void reuseaddr(int fd)
{
    int op = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&op, sizeof(op));
}

static std::vector<std::string> split(const std::string& s, char delimiter = ' ')
{
    std::vector<std::string> v;
    std::string::size_type pos1, pos2;
    pos2 = s.find(delimiter);
    pos1 = 0;
    while (pos2 != std::string::npos) {
        v.push_back(s.substr(pos1, pos2 - pos1));
        pos1 = pos2 + 1;
        pos2 = s.find(delimiter, pos1);
    }   
    v.push_back(s.substr(pos1));
    return v;
}

static bool fileexists(const std::string& file)
{
    int ret = access(file.c_str(), F_OK | R_OK);
    return (ret == 0); 
}

                                                                                                                      
int filesize(const std::string& file)                                                                                 
{                                                                                                                     
    struct stat buf;                                                                                                  
    int ret = stat(file.c_str(), &buf);                                                                               
    if (ret != 0) {                                                                                                   
        return -errno;                                                                                                
    }                                                                                                                 
    return (int)buf.st_size;                                                                                          
}                                                                                                                     
      

bool startsWith(const std::string& s, const std::string& n)
{
    if (s.length() < n.length()) {
        return false;
    }
    if (n.length() == 0) {
        return false;
    }
    return s.substr(0, n.length()) == n;
}

static void send_file(int sock, const std::string& filename)
{
    printf("send file to %d: %s\n", sock, filename.c_str());
    int fd = open(filename.c_str(), O_RDONLY);
    while (true) {
        char buf[1024];
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) {
            break;
        }
        write(sock, buf, n);
    }
    close(fd);
}

static void send_header(int sock, int code, int length = -1)
{
    std::stringstream oss;
    oss << "HTTP/1.1 " << code << " CODE\r\n";
    oss << "Connection: close\r\n";
    oss << "Content-Type: text/html\r\n";
    if (length > 0) {
        oss << "Content-Length: " << length << "\r\n";
    }
    oss << "\r\n";

    write(sock, oss.str().c_str(), oss.str().length());
}

static void send_complete(int sock)
{
    write(sock, "\r\n", 2);
}

static void send_data(int sock, const char * buf, int len)
{
    write(sock, buf, len);
}

static void remove_timeout_client()
{
}

static bool wakeup_client(const std::string& id)
{
    std::map<std::string, AliveClient>::iterator it = gAliveClients.find(id);
    if (it == gAliveClients.end()) {
        return false;
    }
    sendto(alive_socket, "wakeup", 6, 0, (struct sockaddr *)&(it->second.addr), sizeof(struct sockaddr_in));
    printf("wakeup client %s(%s:%d).\n", it->second.id.c_str(), inet_ntoa(it->second.addr.sin_addr), ntohs(it->second.addr.sin_port));
    it->second.status = "Waking";
    return true;
}

static void on_api(int sock, const std::string& url)
{
    // /api/list
    // /api/wakeup/ID

    std::string s = url.substr(1);
    std::vector<std::string> v = split(s, '/');
    if (v.size() < 2) {
        send_header(sock, 404);
        send_complete(sock);
        return;
    }
    if (v[1] == "list") {
        remove_timeout_client();

        std::stringstream oss;
        oss << "{\"op\": \"list\", \"data\": [";
        for (std::map<std::string, AliveClient>::iterator it = gAliveClients.begin(); it != gAliveClients.end(); it++) {
            if (it != gAliveClients.begin()) {
                oss << ", ";
            }
            oss << "{"
                << "\"id\": \"" << it->second.id << "\""
                << ", "
                <<"\"status\": \"" << it->second.status << "\""
                << ", "
                << "\"alive\": " << it->second.alive
                << "}";
        }
        oss << "]}";

        send_header(sock, 200, oss.str().length());
        send_data(sock, oss.str().c_str(), oss.str().length());
        send_complete(sock);

    } else if (v[1] == "wakeup") {
        if (v.size() < 3) {
            send_header(sock, 404);
            send_complete(sock);
        }
        bool b = wakeup_client(v[2]);
        const char * success = "{\"op\": \"wakeup\", \"data\": \"success\"}";
        const char * fail    = "{\"op\": \"wakeup\", \"data\": \"fail\"}";
        const char * result = b ? success : fail;
        send_header(sock, 200, strlen(result));
        send_data(sock, success, strlen(result));
        send_complete(sock);
    }

}


static int on_message_begin(http_parser * parser)
{
    printf("%s\n", __func__);

    HttpClient * client = (HttpClient *)parser->data;
    client->url = "";
    client->complete = false;

    return 0;
}

static int on_url(http_parser* parser, const char * at, size_t length)
{
    printf("%s: %.*s\n", __func__, (int)length, at);

    HttpClient * client = (HttpClient *)parser->data;
    char * s = new char[length + 1];
    memcpy(s, at, length);
    s[length] = '\0';
    client->url += s;
    delete s;

    return 0;
}

static int on_status(http_parser* parser, const char * at, size_t length)
{
    fprintf(stderr, "%s: %.*s\n", __func__, (int)length, at);
    return 0;
}

static int on_header_field(http_parser* parser, const char * at, size_t length)
{
    fprintf(stderr, "%s: %.*s\n", __func__, (int)length, at);
    return 0;
}

static int on_header_value(http_parser* parser, const char * at, size_t length)
{
    fprintf(stderr, "%s: %.*s\n", __func__, (int)length, at);
    return 0;
}

static int on_header_complete(http_parser* parser, const char * at, size_t length)
{
    fprintf(stderr, "%s: %.*s\n", __func__, (int)length, at);
    return 0;
}

static int on_headers_complete(http_parser * parser)
{
    fprintf(stderr, "%s\n", __func__);
    return 0;
}

static int on_body(http_parser* parser, const char * at, size_t length)
{
    fprintf(stderr, "%s: %.*s\n", __func__, (int)length, at);
    return 0;
}

static int on_message_complete(http_parser * parser)
{
    printf("%s\n", __func__);

    HttpClient * client = (HttpClient *)parser->data;
    client->complete = true;
    return 0;
}

static int on_chunk_header(http_parser * parser)
{
    fprintf(stderr, "%s\n", __func__);
    return 0;
}

static int on_chunk_complete(http_parser * parser)
{
    fprintf(stderr, "%s\n", __func__);
    return 0;
}


int main()
{
    http_parser_settings settings;
    settings.on_url = on_url;
    settings.on_body = on_body;
    settings.on_chunk_complete = on_chunk_complete;
    settings.on_chunk_header = on_chunk_header;
    settings.on_header_field = on_header_field;
    settings.on_header_value = on_header_value;
    settings.on_headers_complete = on_headers_complete;
    settings.on_message_begin = on_message_begin;
    settings.on_message_complete = on_message_complete;
    settings.on_status = on_status;

    int epollfd = epoll_create(1024);
    int http_socket = socket(PF_INET, SOCK_STREAM, 0);
    alive_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    assert(epollfd >= 0);
    assert(http_socket >= 0);
    assert(alive_socket >= 0);

    reuseaddr(http_socket);
    reuseaddr(alive_socket);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(1080);

    int ret = bind(http_socket, (struct sockaddr *)&addr, sizeof(addr));
    assert(ret == 0);

    addr.sin_port = htons(1081);
    ret = bind(alive_socket, (struct sockaddr *)&addr, sizeof(addr));
    assert(ret == 0);

    ret = listen(http_socket, 20);
    assert(ret == 0);

    addfd(epollfd, http_socket);
    addfd(epollfd, alive_socket);

    while (true) {
        struct epoll_event events[20];
        long timeout = 1000;
        int nfds = epoll_wait(epollfd, events, 20, timeout);
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == http_socket) {
                socklen_t len = sizeof(addr);
                int fd = accept(http_socket, (struct sockaddr *)&addr, &len);
                addfd(epollfd, fd);
            } else if (events[i].data.fd == alive_socket) {
                AlivePacket packet;
                socklen_t l = sizeof(addr);
                int r = recvfrom(alive_socket, (char *)&packet, sizeof(packet), 0, (struct sockaddr *)&addr, &l);

                assert (r == sizeof(packet));
                gAliveClients[packet.id].id = packet.id;
                gAliveClients[packet.id].status = (packet.on != 0 ? "ON" : "OFF");
                gAliveClients[packet.id].alive = now();
                memcpy(&(gAliveClients[packet.id].addr), &addr, sizeof(addr));

                printf("%s update alive time.\n", packet.id);

            } else {
                int fd = events[i].data.fd;
                char buffer[4096];
                int r = read(fd, buffer, sizeof(buffer));

                std::map<int, HttpClient*>::iterator it = gHttpClients.find(fd);
                if (r <= 0) {
                    removefd(epollfd, fd);
                    if (it != gHttpClients.end()) {
                        HttpClient * client = it->second;
                        delete client->parser;
                        delete client;
                        gHttpClients.erase(it);
                    }
                    continue;
                }
                HttpClient * client = NULL;
                if (it == gHttpClients.end()) {
                    client = new HttpClient();
                    client->parser = new http_parser();
                    client->parser->data = (void*)client;
                    gHttpClients[fd] = client;
                    http_parser_init(client->parser, HTTP_REQUEST);
                } else {
                    client = it->second;
                }
                size_t parsed = http_parser_execute(client->parser, &settings, buffer, r);
                printf("%lu bytes parsed.\n", parsed);
                if (client->complete) {
                    printf("complete\n");
                    std::string& url = client->url;
                    if (startsWith(url, "/api/")) {
                        on_api(fd, url);
                    } else {
                        if (url == "/") {
                            url = "/index.htm";
                        }
                        std::string file = "./www";
                        file += url;
                        if (fileexists(file)) {
                            int size = filesize(file);
                            send_header(fd, 200, size);
                            send_file(fd, file);
                            send_complete(fd);
                        } else {
                            send_header(fd, 404);
                            send_complete(fd);
                        }
                    }
                }
            } // socket type.
        } // for nfds
    } // while true

    close(epollfd);
    close(http_socket);
    close(alive_socket);
    return 0;
}

