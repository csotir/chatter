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

static std::vector<char> msg_buffer(kMaxDataSize);

Server::Server(const char* port)
{
    srand(time(nullptr));
    MakeConnection(port);
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

    if (listen(server_fd_, BACKLOG) == -1)
    {
        perror("chatter-server: listen");
        exit(EXIT_FAILURE);
    }

    AddRoom("global");
    client_pfds_.push_back({server_fd_, POLLIN, 0});
}

void Server::AddRoom(const std::string& name)
{
    if (rooms_.find(name) == rooms_.end())
    {
        Room room(name);
        rooms_.emplace(name, room);
    }
}

void Server::AddClientToRoom(const std::string& room, Client& client)
{
    if (client.room != room)
    {
        if (client.room != "")
        {
            SendToClient(client, "Leaving room: " + client.room + "\r\n");
            rooms_.at(client.room).RemoveClient(client);
        }
        rooms_.at(room).AddClient(client);
        client.room = room;
        SendToClient(client, "Joined room: " + room + "\r\n");
    }
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
        AddClientToRoom("global", client);
        clients_.emplace(client_fd, client);
        client_pfds_.push_back({client_fd, POLLIN, 0});
        printf("New connection from %s on socket %d\r\n", client.addr.c_str(), client_fd);
    }
}

void Server::DisconnectClient(int client_fd, int index)
{
    Client client = clients_.at(client_fd);
    printf("Disconnected %s from socket %d\r\n", client.addr.c_str(), client_fd);
    close(client_fd);
    rooms_.at(client.room).RemoveClient(client);
    SendToServer(client.name + "@" + client.addr + " has disconnected!\r\n");
    client_pfds_.erase(client_pfds_.begin() + index);
    clients_.erase(client_fd);
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
            int client_fd = client_pfds_[i].fd;
            if (client_fd == server_fd_)
            {
                ConnectClient();
            }
            else
            {
                std::string send_str;
                if (ReceiveMessage(client_fd, send_str) == 0)
                {
                    DisconnectClient(client_fd, i);
                    continue;
                }
                else if (send_str[0] > 31)
                {
                    HandleMessage(client_fd, send_str);
                }
            }
        }
        ++i;
    }
}

std::string Server::GetClientAddr(int client_fd)
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

int Server::ReceiveMessage(int client_fd, std::string& send_str)
{
    int nbytes;
    memset(&msg_buffer[0], 0, kMaxDataSize);
    while ((nbytes = recv(client_fd, &msg_buffer[0], msg_buffer.size(), 0)) != -1)
    {
        if (nbytes == 0)
        {
            break;
        }
        send_str.append(msg_buffer.cbegin(), msg_buffer.cend());
        memset(&msg_buffer[0], 0, kMaxDataSize);
    }
    send_str = send_str.c_str(); // trim null chars
    return nbytes;
}

void Server::HandleMessage(int client_fd, std::string& send_str)
{
    if (send_str[0] != '/')
    { // sign and send message
        send_str.insert(0, "[" + clients_.at(client_fd).name + "@" + clients_.at(client_fd).addr + "] ");
        if (send_str.find("\r\n", send_str.size() - 2) == std::string::npos)
        {
            send_str.append("\r\n");
        }
        rooms_.at(clients_.at(client_fd).room).BroadCastMsg(client_fd, send_str);
    }
    else
    {
        send_str = send_str.substr(1, std::string::npos);
        ParseCommand(clients_.at(client_fd), send_str);
    }
}

void Server::SendToClient(const Client& client, const std::string& message)
{
    if (send(client.fd, message.c_str(), message.size(), 0) == -1)
    {
        perror("send");
    }
}

void Server::SendToServer(const std::string& message)
{
    for (auto& [_, room] : rooms_)
    {
        room.BroadCastMsg(server_fd_, message);
    }
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
    if (tokens.size() > 0 && commands.find(tokens[0]) != commands.end())
    {
        switch (commands.at(tokens.at(0)))
        {
            case Command::NAME:
            {
                if (tokens.size() > 1)
                {
                    std::string out = client.name + "@" + client.addr + " is now known as ";
                    client.name = tokens[1];
                    SendToClient(client, "Your new name is " + client.name + ".\r\n");
                    out.append(client.name + "@" + client.addr + ".\r\n");
                    rooms_.at(client.room).BroadCastMsg(client.fd, out);
                }
                else
                {
                    SendToClient(client, "Please specify a new name.\r\n");
                }
                break;
            }
            case Command::WHO:
            {
                std::string out = "Members in current room (" + client.room + "):\r\n";
                for (auto member : rooms_.at(client.room).GetClients())
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
                    if (client.room == tokens[1])
                    {
                        SendToClient(client, "You are already in that room.\r\n");
                    }
                    else
                    {
                        AddRoom(tokens[1]);
                        AddClientToRoom(tokens[1], client);
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
                if (client.room == "global")
                {
                    SendToClient(client, "You are already in the global room.\r\n");
                }
                else
                {
                    AddClientToRoom("global", client);
                }
                break;
            }
            case Command::RANDOM:
            {
                std::string out = "Random! " + client.name + "@" + client.addr +
                    " rolled a " + std::to_string(rand() % 100) + ".\r\n";
                rooms_.at(client.room).BroadCastMsg(server_fd_, out);
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
