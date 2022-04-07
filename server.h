#ifndef _SERVER_H
#define _SERVER_H

#include <poll.h>
#include <vector>
#include "common.h"

#define BACKLOG 10

class Server
{
    public:
        int makeConnection();
        void pollClients();
        void connectClient();
        string makeSendString(const char* addr, vector<char>& buffer);
    private:
        int server;
        vector<pollfd> clients;
};

#endif