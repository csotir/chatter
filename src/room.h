#ifndef CHATTER_ROOM_H_
#define CHATTER_ROOM_H_

#include <string>
#include <unordered_set>
#include <fstream>

#include "client.h"

#ifdef _WIN32
    #include <winsock2.h>
    typedef SOCKET sock_t;
#else
    typedef int sock_t;
#endif

namespace chatter {

class Server;

class Room
{
    public:
        Room(Server& server, const std::string& room_name, const std::string& password, bool enable_logs);
        bool AddMember(const Client& client, const std::string& password = "");
        void RemoveMember(const Client& client);
        void BroadCastMessage(sock_t sender_fd, const char* color, const std::string& message);
        const std::unordered_set<sock_t>& GetMembers() const { return member_fds_; }
    private:
        std::string name_;
        std::string password_;
        std::unordered_set<sock_t> member_fds_;
        std::ofstream log_file_;
        Server* server_;
};

} // namespace chatter

#endif // CHATTER_ROOM_H_
