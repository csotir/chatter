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
        SendToServer(client.name + "@" + client.addr + " has connected!\r\n");
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
    rooms_.at(client.room_name).RemoveClient(client);
    SendToServer(client.name + "@" + client.addr + " has disconnected!\r\n");
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
            rooms_.at(client.room_name).RemoveClient(client);
        }
        rooms_.at(room_name).AddClient(client);
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
                        message.insert(0, "[" + client.name + "@" + client.addr + "] ");
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
        message.append("\r\n");
    }
    return nbytes;
}

void Server::ParseCommand(Client& client, std::string& command)
{
    // TODO: only clean/tokenize the first word at this point, for direct message support
    std::vector<std::string> tokens;
    for (int i = 0; i < command.size();)
    { // clean command string
        if (command[i] < 32)
        {
            command.erase(command.begin() + i);
            continue;
        }
        if (!isalnum(command[i]) && command[i] != ' ')
        {
            SendToClient(client, "Invalid command.\r\n");
            return;
        }
        command[i] = tolower(command[i]);
        ++i;
    }
    int pos = 0;
    while ((pos = command.find(' ')) != std::string::npos)
    { // tokenize command
        if (pos > 1)
        {
            tokens.push_back(command.substr(0, pos));
        }
        command.erase(0, pos + 1);
    }
    if (command.size() > 0)
    {
        tokens.push_back(command);
    }

    /* COMMANDS */
    if (tokens.size() > 0 && chatter::Commands.find(tokens[0]) != chatter::Commands.end())
    {
        switch (chatter::Commands.at(tokens.at(0)))
        {
            case Command::NAME:
            {
                if (tokens.size() > 1)
                {
                    std::string out = client.name + "@" + client.addr + " is now known as ";
                    client.name = tokens[1];
                    SendToClient(client, "Your new name is " + client.name + ".\r\n");
                    out.append(client.name + "@" + client.addr + ".\r\n");
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
                std::string out = "Members in current room (" + client.room_name + "):\r\n";
                for (const auto& member : rooms_.at(client.room_name).GetClients())
                {
                    out += clients_.at(member).name + '@' + clients_.at(member).addr + "\r\n";
                }
                SendToClient(client, out);
                break;
            }
            case Command::JOIN:
            {
                if (tokens.size() > 1)
                {
                    if (client.room_name == tokens[1])
                    {
                        SendToClient(client, "You are already in that room.\r\n");
                    }
                    else
                    {
                        AddClientToRoom(client, tokens[1]);
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
            case Command::RANDOM:
            {
                std::string out = "Random! " + client.name + "@" + client.addr +
                    " rolled a " + std::to_string(rand() % 100) + ".\r\n";
                rooms_.at(client.room_name).BroadCastMessage(server_fd_, out);
                break;
            }
            case Command::HELP:
            {
                std::string out;
                for (const auto& help_text : chatter::Help)
                {
                    out.append(help_text + "\r\n");
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
