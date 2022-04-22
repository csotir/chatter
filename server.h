#ifndef _SERVER_H
#define _SERVER_H

#include <string>
#include <unordered_map>
#include <vector>

#include <poll.h>

#include "client.h"
#include "room.h"

#define BACKLOG 10

enum class Command
{
    NAME,
    WHO,
    JOIN,
    LEAVE,
};

const std::unordered_map<std::string, Command> commands
{
    {"name", Command::NAME},
    {"who", Command::WHO},
    {"join", Command::JOIN},
    {"leave", Command::LEAVE},
};

class Server
{
    public:
        int makeConnection();
        void addRoom(const std::string& name);
        void addClientToRoom(const std::string& room, Client& client);
        void connectClient();
        void pollClients();
        std::string getClientAddr(int client);
        void sendToClient(const Client& client, const std::string& message);
        void parseCommand(Client& client, std::string& command);
    private:
        int server;
        std::vector<pollfd> client_pfds;
        std::unordered_map<int, Client> clients;
        std::unordered_map<std::string, Room> rooms;
};

#endif
