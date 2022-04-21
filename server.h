#ifndef _SERVER_H
#define _SERVER_H

#include <string>
#include <unordered_map>
#include <vector>

#include <poll.h>

#include "client.h"
#include "room.h"

#define BACKLOG 10

enum Command
{
    NAME,
    WHO,
};

const std::unordered_map<std::string, Command> commands
{
    {"name", NAME},
    {"who", WHO},
};

class Server
{
    public:
        int makeConnection();
        void addRoom(const std::string& name);
        void addClientToRoom(const std::string& room, Client& client);
        void connectClient();
        void pollClients();
        std::string getClientName(int client);
        void parseCommand(Client& client, std::string& command);
        void sendToClient(const Client& client, const std::string& message);
    private:
        int server;
        std::vector<pollfd> client_pfds;
        std::unordered_map<int, Client> clients;
        std::unordered_map<std::string, Room> rooms;
};

#endif
