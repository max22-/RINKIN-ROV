#include <stdexcept>
#include <cstring>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include "udp.h"

UDP& UDP::get_instance() {
    static UDP instance("192.168.4.1", 1234);
    return instance;
}

UDP::UDP(const char *ip, uint16_t port) {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock == -1) throw std::runtime_error("failed to create socket");
    memset(&in_addr, 0, sizeof(SOCKADDR_IN));
    if(inet_pton(AF_INET, ip, &in_addr.sin_addr) != 1)
        throw std::runtime_error("invalid ip address");
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(port);
}

UDP::~UDP() {
    closesocket(sock);
}

bool UDP::send(const char *buf, size_t len) {
    return sendto(sock, buf, len, 0, (SOCKADDR*)&in_addr, sizeof(SOCKADDR_IN)) == (ssize_t)len;
}

bool UDP::send(const char *s) {
    return send(s, strlen(s));
}

bool UDP::send(std::string s) {
    return send(s.c_str(), s.length());
}

std::string UDP::receive() {
    char buf[1500] = {0};
    socklen_t inaddr_size = sizeof(SOCKADDR_IN);
    ssize_t bytes = recvfrom(sock, buf, sizeof(buf), 0, (SOCKADDR*)&in_addr, &inaddr_size);
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