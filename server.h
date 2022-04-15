#ifndef _SERVER_H
#define _SERVER_H

#include <string>
#include <unordered_map>
#include <vector>

#include <poll.h>

#include "client.h"
#include "room.h"

#define BACKLOG 10

class Server
{
    public:
        int makeConnection();
        void addRoom(std::string name);
        void addClientToRoom(std::string room, Client client);
        void connectClient();
        void pollClients();
        std::string getClientName(int client);
        std::string makeSendString(Client client, std::vector<char>& buffer);
    private:
        int server;
        std::vector<pollfd> client_pfds;
        std::unordered_map<int, Client> clients;
        std::unordered_map<std::string, Room> rooms;
};

#endif