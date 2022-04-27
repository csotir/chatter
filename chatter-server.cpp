#include <iostream>

#include "server.h"

int main()
{
    chatter::Server chatter;
    chatter.MakeConnection();

    std::cout << "Waiting for clients...\n";

    while (true)
    {
        chatter.PollClients();
    }

    return 0;
}
