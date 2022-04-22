#ifndef _CLIENT_H
#define _CLIENT_H

#include <string>

struct Client
{
    int fd;
    std::string name = "anon";
    std::string addr;
    std::string room;
};

#endif
