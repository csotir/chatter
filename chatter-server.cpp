#include <cstdio>

#include "server.h"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: chatter-server <port>\r\n");
        return 1;
    }

    chatter::Server chatter(argv[1]);

    printf("Waiting for clients...\r\n");

    while (true)
    {
        chatter.PollClients();
    }

    return 0;
}
