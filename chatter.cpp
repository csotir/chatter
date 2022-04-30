#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"

int main(int argc, char* argv[])
{
    int sockfd, bufferSize;
    char buffer[chatter::kMaxDataSize];
    struct addrinfo hints, *servinfo, *p;
    int ret;
    char str[INET6_ADDRSTRLEN];

    if (argc != 3)
    {
        fprintf(stderr, "usage: chatter <address> <port>\r\n");
        return 1;
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((ret = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\r\n", gai_strerror(ret));
        return 1;
    }

    for (p = servinfo; p != nullptr; p = p->ai_next)
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

    if (p == nullptr)
    {
        fprintf(stderr, "chatter: failed to connect\r\n");
        return 1;
    }

    inet_ntop(p->ai_family, chatter::get_in_addr(p->ai_addr), str, sizeof str);
    printf("chatter: connecting to %s\r\n", str);

    freeaddrinfo(servinfo);

    if ((bufferSize = recv(sockfd, buffer, chatter::kMaxDataSize-1, 0)) == -1)
    {
        perror("recv");
        return 1;
    }

    buffer[bufferSize] = '\0';
    printf("chatter: received '%s'\r\n", buffer);

    close(sockfd);

    return 0;
}
