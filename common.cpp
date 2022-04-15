#include <netinet/in.h>

#include "common.h"

void* get_in_addr(sockaddr* sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((sockaddr_in*)sa)->sin_addr);
    }
    return &(((sockaddr_in6*)sa)->sin6_addr);
}