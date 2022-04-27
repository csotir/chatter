#ifndef CHATTER_CLIENT_H_
#define CHATTER_CLIENT_H_

#include <string>

namespace chatter {

struct Client
{
    int fd;
    std::string name = "anon";
    std::string addr;
    std::string room;
};

} // namespace chatter

#endif // CHATTER_CLIENT_H_
