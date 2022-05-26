#include "server.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <stdexcept>

#include "common.h"

namespace chatter {

Server::Server(const char* port) : message_buffer_(chatter::MaxDataSize)
{
    srand(time(nullptr));
    MakeConnection(port);
}

std::string Server::GetClientAddr(int client_fd) const
{
    char addr_buffer[INET6_ADDRSTRLEN];
    sockaddr_storage client_addr;
    socklen_t addr_size = sizeof client_addr;
    getpeername(client_fd, reinterpret_cast<sockaddr*>(&client_addr), &addr_size);
    inet_ntop(client_addr.ss_family,
        get_in_addr(reinterpret_cast<sockaddr*>(&client_addr)),
        addr_buffer, sizeof addr_buffer);
    return std::string(addr_buffer);
}

void Server::MakeConnection(const char* port)
{
    addrinfo hints;
    addrinfo* servinfo;
    int ret;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((ret = getaddrinfo(nullptr, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\r\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    addrinfo* p;
    int yes = 1;
    for (p = servinfo; p != nullptr; p = p->ai_next)
    {
        if ((server_fd_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("chatter-server: socket");
            continue;
        }
        if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }
        if (bind(server_fd_, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(server_fd_);
            perror("chatter-server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);

    if (p == nullptr)
    {
        fprintf(stderr, "chatter-server: failed to bind\r\n");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd_, chatter::Backlog) == -1)
    {
        perror("chatter-server: listen");
        exit(EXIT_FAILURE);
    }

    client_pfds_.push_back({server_fd_, POLLIN, 0});
}

void Server::ConnectClient()
{
    sockaddr_storage client_addr;
    socklen_t addr_size = sizeof client_addr;
    int client_fd = accept(server_fd_, reinterpret_cast<sockaddr*>(&client_addr), &addr_size);
    if (client_fd == -1)
    {
        perror("chatter-server: accept");
    }
    else
    {
        fcntl(client_fd, F_SETFL, O_NONBLOCK);
        Client client;
        client.fd = client_fd;
        client.addr = GetClientAddr(client_fd);
        SendToServer("[" + std::to_string(client.fd) + "]" + client.name + " has connected!\r\n");
        SendToClient(client, "Welcome! You are #" + std::to_string(client.fd) + ".\r\n");
        AddClientToRoom(client, "global");
        clients_.emplace(client_fd, client);
        client_pfds_.push_back({client_fd, POLLIN, 0});
        printf("New connection from %s on socket %d\r\n", client.addr.c_str(), client_fd);
    }
}

void Server::DisconnectClient(int client_fd, int index)
{
    Client& client = clients_.at(client_fd);
    printf("Disconnected %s from socket %d\r\n", client.addr.c_str(), client_fd);
    close(client_fd);
    rooms_.at(client.room_name).RemoveMember(client);
    SendToServer("[" + std::to_string(client.fd) + "]" + client.name + " has disconnected!\r\n");
    client_pfds_.erase(client_pfds_.begin() + index);
    clients_.erase(client_fd);
}

void Server::AddClientToRoom(Client& client, const std::string& room_name)
{
    if (client.room_name != room_name)
    {
        rooms_.emplace(room_name, Room(room_name));
        if (client.room_name != "")
        {
            SendToClient(client, "Leaving room: " + client.room_name + "\r\n");
            rooms_.at(client.room_name).RemoveMember(client);
            if (rooms_.at(client.room_name).GetMembers().empty())
            {
                rooms_.erase(client.room_name);
            }
        }
        rooms_.at(room_name).AddMember(client);
        client.room_name = room_name;
        SendToClient(client, "Joined room: " + room_name + "\r\n");
    }
}

void Server::SendToClient(const Client& client, const std::string& message) const
{
    if (send(client.fd, message.c_str(), message.size(), 0) == -1)
    {
        perror("send");
    }
}

void Server::SendToServer(const std::string& message) const
{
    for (const auto& [_, room] : rooms_)
    {
        room.BroadCastMessage(server_fd_, message);
    }
}

void Server::PollClients()
{
    int poll_count = poll(&client_pfds_[0], static_cast<nfds_t>(client_pfds_.size()), -1);
    if (poll_count == -1)
    {
        perror("poll");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < client_pfds_.size();)
    {
        if (client_pfds_[i].revents & POLLIN)
        {
            if (client_pfds_[i].fd == server_fd_)
            {
                ConnectClient();
            }
            else
            {
                Client& client = clients_.at(client_pfds_[i].fd);
                std::string message;
                if (ReceiveMessage(client.fd, message) == 0)
                {
                    DisconnectClient(client.fd, i);
                    continue;
                }
                else if (message[0] > 31)
                {
                    if (message[0] != '/')
                    {
                        message.insert(0, "[" + std::to_string(client.fd) + "]" + client.name + ": ");
                        rooms_.at(client.room_name).BroadCastMessage(client.fd, message);
                    }
                    else
                    {
                        message = message.substr(1, std::string::npos);
                        ParseCommand(client, message);
                    }
                }
            }
        }
        ++i;
    }
}

int Server::ReceiveMessage(int client_fd, std::string& message)
{
    int nbytes;
    memset(&message_buffer_[0], 0, chatter::MaxDataSize);
    while ((nbytes = recv(client_fd, &message_buffer_[0], message_buffer_.size(), 0)) != -1)
    {
        if (nbytes == 0)
        {
            break;
        }
        message.append(message_buffer_.cbegin(), message_buffer_.cend());
        memset(&message_buffer_[0], 0, chatter::MaxDataSize);
    }
    message = message.c_str(); // trim null chars
    if (message.find("\r\n", message.size() - 2) == std::string::npos)
    {
        message += "\r\n";
    }
    return nbytes;
}

std::string Server::GetToken(std::string& message) const
{
    std::string ret;
    int pos = 0;
    while (ret.empty() && (pos = message.find(' ')) != std::string::npos)
    {
        ret += message.substr(0, pos);
        message.erase(0, pos + 1);
    }
    if (ret.empty())
    {
        ret = message;
        message.erase(0, std::string::npos);
    }
    return ret;
}

bool Server::SanitizeString(std::string& str, bool lower) const
{
    for (int i = 0; i < str.size();)
    {
        if (str[i] < 32)
        {
            str.erase(str.begin() + i);
            continue;
        }
        if (!isalpha(str[i]))
        {
            str.erase(0, std::string::npos);
            return false;
        }
        if (lower)
        {
            str[i] = tolower(str[i]);
        }
        ++i;
    }
    return true;
}

void Server::ParseCommand(Client& client, std::string& message)
{
    std::string command = GetToken(message);
    if (!SanitizeString(command, true))
    {
        SendToClient(client, "Invalid command.\r\n");
        return;
    }

    /* COMMANDS */
    if (chatter::Commands.find(command) != chatter::Commands.end())
    {
        switch (chatter::Commands.at(command))
        {
            case Command::NAME:
            {
                std::string new_name = GetToken(message);
                if (!SanitizeString(new_name))
                {
                    SendToClient(client, "Invalid name.\r\n");
                    return;
                }
                if (!new_name.empty())
                {
                    std::string out = "[" + std::to_string(client.fd) + "]" + client.name + " is now known as ";
                    client.name = new_name;
                    SendToClient(client, "Your new name is " + client.name + ".\r\n");
                    out += client.name + ".\r\n";
                    rooms_.at(client.room_name).BroadCastMessage(client.fd, out);
                }
                else
                {
                    SendToClient(client, "Please specify a new name.\r\n");
                }
                break;
            }
            case Command::WHO:
            {
                std::string room_name = GetToken(message);
                std::string out;
                if (!SanitizeString(room_name, true))
                {
                    SendToClient(client, "Invalid room name.\r\n");
                    return;
                }
                if (!room_name.empty())
                {
                    if (rooms_.find(room_name) == rooms_.end())
                    {
                        SendToClient(client, "Room \"" + room_name + "\" doesn't exist!\r\n");
                        return;
                    }
                    else
                    {
                        room_name = room_name;
                    }
                }
                else
                {
                    room_name = client.room_name;
                }
                out += "Members of room \"" + room_name + "\":\r\n";
                for (auto member : rooms_.at(room_name).GetMembers())
                {
                    out += "[" + std::to_string(clients_.at(member).fd) + "]" + clients_.at(member).name;
                    if (member == client.fd)
                    {
                        out += " (you)";
                    }
                    out += "\r\n";
                }
                SendToClient(client, out);
                break;
            }
            case Command::ROOMS:
            {
                std::string out = "Rooms (members):\r\n";
                for (const auto& room : rooms_)
                {
                    out += room.first + " (" + std::to_string(room.second.GetMembers().size()) + ")\r\n";
                }
                SendToClient(client, out);
                break;
            }
            case Command::JOIN:
            {
                std::string new_room = GetToken(message);
                if (!SanitizeString(new_room, true))
                {
                    SendToClient(client, "Invalid room name.\r\n");
                    return;
                }
                if (!new_room.empty())
                {
                    if (client.room_name == new_room)
                    {
                        SendToClient(client, "You are already in that room.\r\n");
                    }
                    else
                    {
                        AddClientToRoom(client, new_room);
                    }
                }
                else
                {
                    SendToClient(client, "Please specify a room to join.\r\n");
                }
                break;
            }
            case Command::LEAVE:
            {
                if (client.room_name == "global")
                {
                    SendToClient(client, "You are already in the global room.\r\n");
                }
                else
                {
                    AddClientToRoom(client, "global");
                }
                break;
            }
            case Command::TELL:
            {
                int dest_fd;
                try
                {
                    dest_fd = std::stoi(GetToken(message));
                }
                catch (...)
                {
                    SendToClient(client, "Please enter a valid recipient number.\r\n");
                    return;
                }
                if (clients_.find(dest_fd) == clients_.end())
                {
                    SendToClient(client, "User #" + std::to_string(dest_fd) + " does not exist.\r\n");
                }
                else
                {
                    if (!message.empty())
                    {
                        SendToClient(clients_.at(dest_fd), "[" + std::to_string(client.fd) +
                            "]" + client.name + "> " + message);
                    }
                }
                break;
            }
            case Command::RANDOM:
            {
                std::string out = "[" + std::to_string(client.fd) + "]Random! " +
                    client.name + " rolled a " + std::to_string(rand() % 100) + ".\r\n";
                rooms_.at(client.room_name).BroadCastMessage(server_fd_, out);
                break;
            }
            case Command::HELP:
            {
                std::string out;
                for (const auto& help_text : chatter::Help)
                {
                    out += help_text + "\r\n";
                }
                SendToClient(client, out);
                break;
            }
        }
    }
    else
    {
        SendToClient(client, "Unknown command.\r\n");
    }
}

} // namespace chatter
