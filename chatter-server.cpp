#include <iostream>

#include "server.h"

int main()
{
    Server chatter;
    chatter.makeConnection();

    std::cout << "Waiting for clients...\n";

    while (true)
    {
        chatter.pollClients();
    }

    return 0;
}
