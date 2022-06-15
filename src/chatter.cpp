#include <cstdio>
#include <string>

#include "server.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: chatter <port>\r\n");
        return 1;
    }
    bool enable_logs = false;
    if (argc > 2)
    {
        if (std::string(argv[2]) == "-L")
        {
            printf("Logging is enabled!\r\n");
            enable_logs = true;
        }
    }
    chatter::Server chatter(argv[1], enable_logs);

    printf("Waiting for clients on port %s...\r\n", argv[1]);

    while (true)
    {
        chatter.PollClients();
    }

    return 0;
}
