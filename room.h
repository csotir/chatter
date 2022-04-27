#ifndef CHATTER_ROOM_H_
#define CHATTER_ROOM_H_

#include <string>
#include <unordered_set>

#include "client.h"

namespace chatter {

class Room
{
    public:
        Room() = default;
        Room(const std::string& room_name);
        void AddClient(const Client& client);
        void RemoveClient(const Client& client);
        void BroadCastMsg(int sender_fd, const std::string& msg);
        const std::unordered_set<int>& GetClients() { return client_fds_; }
    private:
        std::string name_;
        std::unordered_set<int> client_fds_;
};

} // namespace chatter

#endif // CHATTER_ROOM_H_
