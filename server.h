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
        string makeSendString(string addr, vector<char>& buffer);
        string getClientName(int client);
        void broadCastMsg(int sender, string msg);
    private:
        int server;
        vector<pollfd> clients;
};

#endif