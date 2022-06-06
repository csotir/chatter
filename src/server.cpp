#include "server.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <stdexcept>

namespace chatter {

Server::Server(const char* port) : message_buffer_(chatter::MaxDataSize), command_handler_(*this)
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
        GetInAddr(reinterpret_cast<sockaddr*>(&client_addr)),
        addr_buffer, sizeof addr_buffer);
    return std::string(addr_buffer);
}

void* Server::GetInAddr(sockaddr* sa) const
{
    if (sa->sa_family == AF_INET)
    {
        return &((reinterpret_cast<sockaddr_in*>(sa))->sin_addr);
    }
    return &((reinterpret_cast<sockaddr_in6*>(sa))->sin6_addr);
};

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

void Server::AddClientToRoom(Client& client, const std::string& room_name, const std::string& password)
{
    std::string old_room_name = client.room_name;
    if (old_room_name != room_name)
    {
        rooms_.emplace(room_name, Room(room_name, password));
        if (rooms_.at(room_name).AddMember(client, password))
        {
            if (old_room_name != "")
            {
                SendToClient(client, "Leaving room: " + old_room_name + "\r\n");
                rooms_.at(old_room_name).RemoveMember(client);
                if (rooms_.at(old_room_name).GetMembers().empty())
                {
                    rooms_.erase(old_room_name);
                }
            }
            client.room_name = room_name;
            SendToClient(client, "Joined room: " + room_name + "\r\n");
        }
        else
        {
            SendToClient(client, "Incorrect password!\r\n");
        }
        
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
                        command_handler_.ParseCommand(client, message);
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

} // namespace chatter
