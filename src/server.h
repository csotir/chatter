#ifndef CHATTER_SERVER_H_
#define CHATTER_SERVER_H_

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define close closesocket
    #define ioctl ioctlsocket
    #define poll WSAPoll
    typedef unsigned long int nfds_t;
    typedef SOCKET sock_t;
#else
    #include <poll.h>
    #include <sys/socket.h>
    typedef int sock_t;
#endif

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
        void SendToClient(sock_t client_fd, const std::string& timestamp, const char* color, const std::string& message) const;
        void SendToAllClients(const std::string& timestamp, const std::string& message) const;
        void PollClients();
        std::string GetTimestamp() const;
    private:
        friend class CommandHandler;
        std::string GetClientAddr(sock_t client_fd) const;
        void* GetInAddr(sockaddr* sa) const;
        void MakeConnection(const char* port);
        void ConnectClient();
        void DisconnectClient(sock_t client_fd, size_t index);
        void AddClientToRoom(Client& client, const std::string& room, const std::string& password = "");
        int ReceiveMessage(sock_t client_fd, std::string& message);
        sock_t server_fd_;
        bool logs_enabled_;
        std::vector<pollfd> client_pfds_;
        std::unordered_map<sock_t, Client> clients_;
        std::unordered_map<std::string, Room> rooms_;
        std::vector<char> message_buffer_;
        CommandHandler command_handler_;
};

} // namespace chatter

#endif // CHATTER_SERVER_H_
