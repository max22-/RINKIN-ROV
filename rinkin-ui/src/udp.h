#include <cstdint>
#include <cstddef>
#include <string>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;
#define closesocket close
#endif

class UDP {
public:
    static UDP& get_instance();
    void reset(const char *ip);
    bool send(const char *buf, size_t len);
    bool send(const char *s);
    bool send(std::string s);
    bool data_available();
    std::string receive();

private:
    UDP(const char *ip, uint16_t port);
    ~UDP();
    UDP(const UDP&) = delete;
    UDP& operator=(const UDP&) = delete;
    SOCKET sock = -1;
    SOCKADDR_IN in_addr;
    const uint16_t port;
};