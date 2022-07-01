#ifndef CHATTER_SERVER_H_
#define CHATTER_SERVER_H_

#include <poll.h>
#include <sys/socket.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "client.h"
#include "command_handler.h"
#include "room.h"

namespace chatter {

constexpr int Backlog = 10;
constexpr int MaxDataSize = 100;

class Server
{
    public:
        Server(const char* port, bool enable_logs);
        void SendToClient(int client_fd, const std::string& timestamp, const char* color, const std::string& message) const;
        void SendToAllClients(const std::string& timestamp, const std::string& message) const;
        void PollClients();
        std::string GetTimestamp() const;
    private:
        friend class CommandHandler;
        std::string GetClientAddr(int client_fd) const;
        void* GetInAddr(sockaddr* sa) const;
        void MakeConnection(const char* port);
        void ConnectClient();
        void DisconnectClient(int client_fd, int index);
        void AddClientToRoom(Client& client, const std::string& room, const std::string& password = "");
        int ReceiveMessage(int client_fd, std::string& message);
        int server_fd_;
        bool logs_enabled_;
        std::vector<pollfd> client_pfds_;
        std::unordered_map<int, Client> clients_;
        std::unordered_map<std::string, Room> rooms_;
        std::vector<char> message_buffer_;
        CommandHandler command_handler_;
};

} // namespace chatter

#endif // CHATTER_SERVER_H_
