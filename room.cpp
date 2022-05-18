#include "room.h"

#include <sys/socket.h>

#include <cstdio>

namespace chatter {

Room::Room(const std::string& room_name) : name_(room_name) { }

void Room::AddMember(const Client& client)
{
    member_fds_.insert(client.fd);
    BroadCastMessage(client.fd, client.name + "@" + client.addr + " has joined the room!\r\n");
}

void Room::RemoveMember(const Client& client)
{
    member_fds_.erase(client.fd);
    BroadCastMessage(client.fd, client.name + "@" + client.addr + " has left the room!\r\n");
}

void Room::BroadCastMessage(int sender_fd, const std::string& message) const
{
    for (auto dest_fd : member_fds_)
    {
        if (dest_fd != sender_fd)
        {
            if (send(dest_fd, message.c_str(), message.size(), 0) == -1)
            {
                perror("send");
            }
        }
    }
}

} // namespace chatter
