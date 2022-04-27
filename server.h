#ifndef CHATTER_SERVER_H_
#define CHATTER_SERVER_H_

#include <poll.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "client.h"
#include "room.h"

namespace chatter {

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
        int MakeConnection();
        void AddRoom(const std::string& name);
        void AddClientToRoom(const std::string& room, Client& client);
        void ConnectClient();
        void DisconnectClient(int client_fd, int index, bool orderly);
        void PollClients();
        std::string GetClientAddr(int client_fd);
        int ReceiveMessage(int client_fd);
        void HandleMessage(int client_fd);
        void SendToClient(const Client& client, const std::string& message);
        void ParseCommand(Client& client, std::string& command);
    private:
        int server_fd_;
        std::vector<pollfd> client_pfds_;
        std::unordered_map<int, Client> clients_;
        std::unordered_map<std::string, Room> rooms_;
};

} // namespace chatter

#endif // CHATTER_SERVER_H_
