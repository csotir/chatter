#ifndef _CLIENT_H
#define _CLIENT_H

#include <string>

struct Client
{
    int fd;
    std::string name = "Anon";
    std::string addr;
    std::string room = "global";
};

#endif