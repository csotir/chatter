#include "server.h"

int main()
{
    Server chatter;
    chatter.makeConnection();

    cout << "Waiting for clients...\n";

    while (true)
    {
        chatter.pollClients();
    }

    return 0;
}
