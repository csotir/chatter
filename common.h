#ifndef _COMMON_H
#define _COMMON_H

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

#define PORT "36547"
#define MAXDATASIZE 100

void* get_in_addr(struct sockaddr* sa);

#endif