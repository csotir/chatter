#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"

int main(int argc, char *argv[])
{
    int sockfd, bufferSize;
    char buffer[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int ret;
    char str[INET6_ADDRSTRLEN];

    if (argc != 2)
    {
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((ret = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("chatter: socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("chatter: connect");
            continue;
        }
        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "chatter: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr(p->ai_addr), str, sizeof str);
    printf("chatter: connecting to %s\n", str);

    freeaddrinfo(servinfo);

    if ((bufferSize = recv(sockfd, buffer, MAXDATASIZE-1, 0)) == -1)
    {
        perror("recv");
        exit(1);
    }

    buffer[bufferSize] = '\0';
    printf("chatter: received '%s'\n", buffer);

    close(sockfd);

    return 0;
}
