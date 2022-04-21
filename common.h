#ifndef _COMMON_H
#define _COMMON_H

#include <sys/socket.h>

#define PORT "36547"
#define MAXDATASIZE 100

void* get_in_addr(sockaddr* sa);

#endif
