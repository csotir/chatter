#include "room.h"

#include <sys/socket.h>
#include <ctime>

#include <cstdio>

namespace chatter {

Room::Room(const std::string& room_name, const std::string& password, bool enable_logs)
    : name_(room_name), password_(password)
{
    if (enable_logs)
    {
        time_t now = time(nullptr);
        tm* utc_time = gmtime(&now);
        log_file_.open("logs/" + name_ + ".log", std::fstream::app);
        log_file_ << "Starting new log on " + std::string(asctime(utc_time));
        log_file_.flush();
    }
}

bool Room::AddMember(const Client& client, const std::string& timestamp, const std::string& password)
{
    if (password != password_ && password_ != "")
    {
        return false;
    }
    member_fds_.insert(client.fd);
    BroadCastMessage(client.fd, timestamp + "[" + std::to_string(client.fd) +
        "]" + client.name + " has joined the room!\r\n");
    return true;
}

void Room::RemoveMember(const Client& client, const std::string& timestamp)
{
    member_fds_.erase(client.fd);
    BroadCastMessage(client.fd, timestamp + "[" + std::to_string(client.fd) +
        "]" + client.name + " has left the room!\r\n");
}

void Room::BroadCastMessage(int sender_fd, const std::string& message)
{
    if (log_file_.is_open())
    {
        log_file_ << message;
        log_file_.flush();
    }
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
