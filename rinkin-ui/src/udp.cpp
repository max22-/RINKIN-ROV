#include <stdexcept>
#include <cstring>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include "udp.h"
#include "config.h"

UDP& UDP::get_instance() {
    static UDP instance(config.host.c_str(), 1234);
    return instance;
}

UDP::UDP(const char *host, uint16_t port) : port(port) {
    reset(host);
}

UDP::~UDP() {
    freeaddrinfo(ai_result);
    closesocket(sock);
}

void UDP::reset(const char *host) {
    if(sock != -1) closesocket(sock);
    sock = -1;
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int err = getaddrinfo(config.host.c_str(), std::to_string(port).c_str(), &hints, &ai_result);

    if(err != 0) {
        throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(err));
    }
    struct addrinfo *p;
    for(p = ai_result; p != NULL; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(sock != -1) break;
    }
    if(sock == -1) {
        freeaddrinfo(ai_result);
        throw std::runtime_error("failed to create socket");
    }
    sock_addr_info = p;
}

bool UDP::send(const char *buf, size_t len) {
    return sendto(sock, buf, len, 0, sock_addr_info->ai_addr, sock_addr_info->ai_addrlen) == (ssize_t)len;
}

bool UDP::send(const char *s) {
    return send(s, strlen(s));
}

bool UDP::send(std::string s) {
    return send(s.c_str(), s.length());
}

std::string UDP::receive() {
    char buf[1500] = {0};
    socklen_t inaddr_size = sock_addr_info->ai_addrlen;
    ssize_t bytes = recvfrom(sock, buf, sizeof(buf), 0, sock_addr_info->ai_addr, &inaddr_size);
    if(bytes > 0)
        return std::string(buf, bytes);
    else
        return "";
}

bool UDP::data_available() {
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    return select(sock + 1, &rfds, NULL, NULL, &tv) > 0;
}