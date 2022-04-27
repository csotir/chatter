#include "server.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "common.h"

namespace chatter {

static std::vector<char> msg_buffer(MAXDATASIZE);

int Server::MakeConnection()
{
    addrinfo hints, *servinfo, *p;
    int ret;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((ret = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\r\n", gai_strerror(ret));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((server_fd_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("chatter-server: socket");
            continue;
        }
        if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
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

    if (p == NULL)
    {
        fprintf(stderr, "chatter-server: failed to bind\r\n");
        exit(1);
    }

    if (listen(server_fd_, BACKLOG) == -1)
    {
        perror("chatter-server: listen");
        exit(1);
    }

    AddRoom("global");
    client_pfds_.push_back({server_fd_, POLLIN, 0});

    return 0;
}

void Server::AddRoom(const std::string& name)
{
    if (rooms_.find(name) == rooms_.end())
    {
        Room room(name);
        rooms_[name] = room;
    }
}

void Server::AddClientToRoom(const std::string& room, Client& client)
{
    if (client.room != room)
    {
        if (client.room != "")
        {
            SendToClient(client, "Leaving room: " + client.room + "\r\n");
            rooms_[client.room].RemoveClient(client);
        }
        rooms_[room].AddClient(client);
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
        Client client;
        client.fd = client_fd;
        client.addr = GetClientAddr(client_fd);
        AddClientToRoom("global", client);
        clients_[client_fd] = client;
        client_pfds_.push_back({client_fd, POLLIN, 0});
        printf("New connection from %s on socket %d\r\n", client.addr.c_str(), client_fd);
    }
}

void Server::DisconnectClient(int client_fd, int index, bool orderly)
{
    if (orderly)
    {
        printf("Disconnected %s from socket %d\r\n", clients_[client_fd].addr.c_str(), client_fd);
    }
    else
    {
        perror("recv");
    }
    close(client_fd);
    rooms_[clients_[client_fd].room].RemoveClient(clients_[client_fd]);
    client_pfds_.erase(client_pfds_.begin() + index);
    clients_.erase(client_fd);
}

void Server::PollClients()
{
    int poll_count = poll(&client_pfds_[0], static_cast<nfds_t>(client_pfds_.size()), -1);
    if (poll_count == -1)
    {
        perror("poll");
        exit(1);
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
                int nbytes = ReceiveMessage(client_fd);
                if (nbytes <= 0)
                {
                    DisconnectClient(client_fd, i, nbytes == 0);
                    continue;
                }
                else
                {
                    HandleMessage(client_fd);
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

int Server::ReceiveMessage(int client_fd)
{
    memset(&msg_buffer[0], 0, MAXDATASIZE);
    return recv(client_fd, &msg_buffer[0], msg_buffer.size(), 0);
}

void Server::HandleMessage(int client_fd)
{
    if (msg_buffer[0] > 31)
    { // ignore empty messages
        std::string send_str(msg_buffer.cbegin(), msg_buffer.cend());
        // TODO: probably set socket to nonblocking and loop on recv ret > 0 instead
        while (send_str.back() != '\0' && send_str.back() != '\n')
        { // get whole message
            ReceiveMessage(client_fd);
            send_str.append(msg_buffer.cbegin(), msg_buffer.cend());
        }
        if (send_str[0] != '/')
        { // sign and send message
            send_str.insert(0, clients_[client_fd].name + "@" + clients_[client_fd].addr + "> ");
            send_str = send_str.c_str(); // trim null chars
            if (send_str.find("\r\n", send_str.size() - 2) == std::string::npos)
            {
                send_str.append("\r\n");
            }
            rooms_[clients_[client_fd].room].BroadCastMsg(client_fd, send_str);
        }
        else
        {
            send_str = send_str.substr(1, std::string::npos);
            ParseCommand(clients_[client_fd], send_str);
        }
    }
}

void Server::SendToClient(const Client& client, const std::string& message)
{
    if (send(client.fd, message.c_str(), message.size(), 0) == -1)
    {
        perror("send");
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
                    client.name = tokens[1];
                    SendToClient(client, "Your new name is " + client.name + "!\r\n");
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
                for (auto member : rooms_[client.room].GetClients())
                {
                    out += clients_[member].name + '@' + clients_[member].addr + "\r\n";
                }
                SendToClient(client, out);
                break;
            }
            case Command::JOIN:
            {
                if (tokens.size() > 1)
                {
                    AddRoom(tokens[1]);
                    AddClientToRoom(tokens[1], client);
                }
                else
                {
                    SendToClient(client, "Please specify a room to join.\r\n");
                }
                break;
            }
            case Command::LEAVE:
            {
                AddClientToRoom("global", client);
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
