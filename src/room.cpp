#include "room.h"

#include <ctime>

#include <cstdio>

#include "colors.h"
#include "server.h"

namespace chatter {

Room::Room(Server& server, const std::string& room_name, const std::string& password, bool enable_logs)
    : server_(&server), name_(room_name), password_(password)
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

bool Room::AddMember(const Client& client, const std::string& password)
{
    if (password != password_ && password_ != "")
    {
        return false;
    }
    member_fds_.insert(client.fd);
    BroadCastMessage(client.fd, chatter::colors::Yellow, "[" + std::to_string(client.fd) +
        "]" + client.name + " has joined the room!\r\n");
    return true;
}

void Room::RemoveMember(const Client& client)
{
    member_fds_.erase(client.fd);
    BroadCastMessage(client.fd, chatter::colors::Yellow, "[" + std::to_string(client.fd) +
        "]" + client.name + " has left the room!\r\n");
}

void Room::BroadCastMessage(sock_t sender_fd, const char* color, const std::string& message)
{
    std::string timestamp = server_->GetTimestamp();
    if (log_file_.is_open())
    {
        log_file_ << timestamp << message;
        log_file_.flush();
    }
    for (auto dest_fd : member_fds_)
    {
        if (dest_fd != sender_fd)
        {
            server_->SendToClient(dest_fd, timestamp, color, message);
        }
    }
}

} // namespace chatter
