#ifndef CHATTER_CLIENT_H_
#define CHATTER_CLIENT_H_

#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    typedef SOCKET sock_t;
#else
    typedef int sock_t;
#endif

namespace chatter {

struct Client
{
    sock_t fd;
    std::string name = "anon";
    std::string addr;
    std::string room_name;
    bool color = true;
};

} // namespace chatter

#endif // CHATTER_CLIENT_H_
