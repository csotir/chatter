#ifndef CHATTER_SERVER_H_
#define CHATTER_SERVER_H_

#include <poll.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "client.h"
#include "room.h"

namespace chatter {

constexpr int Backlog = 10;

enum class Command
{
    NAME,
    WHO,
    ROOMS,
    JOIN,
    LEAVE,
    RANDOM,
    HELP,
};

const std::unordered_map<std::string, Command> Commands
{
    {"name", Command::NAME},
    {"who", Command::WHO},
    {"rooms", Command::ROOMS},
    {"join", Command::JOIN},
    {"leave", Command::LEAVE},
    {"random", Command::RANDOM},
    {"help", Command::HELP},
};

const std::vector<std::string> Help
{
    "/name <name> : Change your display name.",
    "/who         : List users in current room.",
    "/who <room>  : List users in specified room.",
    "/rooms       : List rooms.",
    "/join <room> : Join the specified room.",
    "/leave       : Leave the current room.",
    "/random      : Roll a random number from 0 to 99.",
    "/help        : Display available commands.",
};

class Server
{
    public:
        Server(const char* port);
        void SendToServer(const std::string& message) const;
        void PollClients();
    private:
        std::string GetClientAddr(int client_fd) const;
        void MakeConnection(const char* port);
        void ConnectClient();
        void DisconnectClient(int client_fd, int index);
        void AddClientToRoom(Client& client, const std::string& room);
        void SendToClient(const Client& client, const std::string& message) const;
        int ReceiveMessage(int client_fd, std::string& message);
        void ParseCommand(Client& client, std::string& command);
        int server_fd_;
        std::vector<pollfd> client_pfds_;
        std::unordered_map<int, Client> clients_;
        std::unordered_map<std::string, Room> rooms_;
        std::vector<char> message_buffer_;
};

} // namespace chatter

#endif // CHATTER_SERVER_H_
