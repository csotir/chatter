#include "room.h"

#include <sys/socket.h>

#include <cstdio>

namespace chatter {

Room::Room(const std::string& room_name) : name_(room_name) { }

void Room::AddClient(const Client& client)
{
    client_fds_.insert(client.fd);
    std::string msg = client.name + "@" + client.addr + " has joined the room!\r\n";
    BroadCastMsg(client.fd, msg);
}

void Room::RemoveClient(const Client& client)
{
    client_fds_.erase(client.fd);
    std::string msg = client.name + "@" + client.addr + " has left the room!\r\n";
    BroadCastMsg(client.fd, msg);
}

void Room::BroadCastMsg(int sender_fd, const std::string& msg)
{
    for (auto dest_fd : client_fds_)
    {
        if (dest_fd != sender_fd)
        {
            if (send(dest_fd, msg.c_str(), msg.size(), 0) == -1)
            {
                perror("send");
            }
        }
    }
}

} // namespace chatter
