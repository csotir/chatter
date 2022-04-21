#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "server.h"

std::vector<char> msg_buffer(MAXDATASIZE);

int Server::makeConnection()
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
        if ((server = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("chatter-server: socket");
            continue;
        }
        if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }
        if (bind(server, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(server);
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

    if (listen(server, BACKLOG) == -1)
    {
        perror("chatter-server: listen");
        exit(1);
    }

    addRoom("global");
    client_pfds.push_back({server, POLLIN, 0});

    return 0;
}

void Server::addRoom(const std::string& name)
{
    if (rooms.find(name) != rooms.end())
    {
        Room room(name);
        rooms[name] = room;
    }
}

void Server::addClientToRoom(const std::string& room, Client& client)
{
    if (client.room != room)
    {
        rooms[client.room].removeClient(client);
    }
    rooms[room].addClient(client);
    client.room = room;
}

void Server::connectClient()
{
    sockaddr_storage client_addr;
    socklen_t addr_size = sizeof client_addr;
    int client_fd = accept(server, (sockaddr*)&client_addr, &addr_size);
    if (client_fd == -1)
    {
        perror("chatter-server: accept");
    }
    else
    {
        Client client;
        client.fd = client_fd;
        client.addr = getClientName(client_fd);
        addClientToRoom("global", client);
        clients[client_fd] = client;
        client_pfds.push_back({client_fd, POLLIN, 0});
        printf("New connection from %s on socket %d\r\n", client.addr.c_str(), client_fd);
    }
}

void Server::pollClients()
{
    int poll_count = poll(&client_pfds[0], (nfds_t)client_pfds.size(), -1);
    if (poll_count == -1)
    {
        perror("poll");
        exit(1);
    }

    for (int i = 0; i < client_pfds.size();)
    {
        if (client_pfds[i].revents & POLLIN)
        {
            int client_fd = client_pfds[i].fd;
            if (client_fd == server)
            {
                connectClient();
            }
            else
            {
                memset(&msg_buffer[0], 0, MAXDATASIZE);
                int nbytes = recv(client_fd, &msg_buffer[0], msg_buffer.size(), 0);
                if (nbytes <= 0)
                { // client disconnect
                    if (nbytes == 0)
                    {
                        printf("Disconnected %s from socket %d\r\n", clients[client_fd].addr.c_str(), client_fd);
                    }
                    else
                    {
                        perror("recv");
                    }
                    close(client_fd);
                    rooms[clients[client_fd].room].removeClient(clients[client_fd]);
                    client_pfds.erase(client_pfds.begin() + i);
                    clients.erase(client_fd);
                    continue;
                }
                else
                { // client message
                    if (msg_buffer[0] > 31)
                    { // ignore empty messages
                        std::string send_str(msg_buffer.cbegin(), msg_buffer.cend());
                        while (send_str.back() != '\0')
                        { // get whole message
                            memset(&msg_buffer[0], 0, MAXDATASIZE);
                            recv(client_fd, &msg_buffer[0], msg_buffer.size(), 0);
                            send_str.append(msg_buffer.cbegin(), msg_buffer.cend());
                        }
                        if (send_str[0] != '/')
                        { // sign and send message
                            send_str.insert(0, clients[client_fd].name + "@" + clients[client_fd].addr + "> ");
                            send_str = send_str.c_str(); // trim null chars
                            if (send_str.find("\r\n", send_str.size() - 2) == std::string::npos)
                            { // add newline
                                send_str.append("\r\n");
                            }
                            rooms[clients[client_fd].room].broadCastMsg(client_fd, send_str);
                        }
                        else
                        { // parse command
                            send_str = send_str.substr(1, std::string::npos);
                            parseCommand(clients[client_fd], send_str);
                        }
                        
                    }
                }
            }
        }
        i++;
    }
}

std::string Server::getClientName(int client)
{
    char addr_buffer[INET6_ADDRSTRLEN];
    sockaddr_storage client_addr;
    socklen_t addr_size = sizeof client_addr;
    getpeername(client, (sockaddr*)&client_addr, &addr_size);
    inet_ntop(client_addr.ss_family,
        get_in_addr((sockaddr*)&client_addr),
        addr_buffer, sizeof addr_buffer);
    return std::string(addr_buffer);
}

void Server::parseCommand(Client& client, std::string& command)
{
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
            sendToClient(client, "Invalid command.\r\n");
            return;
        }
        command[i] = tolower(command[i]);
        i++;
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
    try
    {
        switch (commands.at(tokens.at(0)))
        {
            case NAME:
            {
                if (tokens.size() > 1)
                {
                    client.name = tokens[1];
                    sendToClient(client, "Your new name is " + client.name + "!\r\n");
                }
                else
                {
                    sendToClient(client, "Please specify a new name.\r\n");
                }
                break;
            }
            case WHO:
            {
                std::string out = "Members in room " + client.room + ":\r\n";
                for (auto member : rooms[client.room].getClients())
                {
                    out += clients[member].name + '@' + clients[member].addr + "\r\n";
                }
                sendToClient(client, out);
                break;
            }
        }
    }
    catch(std::out_of_range& e)
    {
        sendToClient(client, "Unknown command.\r\n");
    }
}

void Server::sendToClient(const Client& client, const std::string& message)
{
    if (send(client.fd, message.c_str(), message.size(), 0) == -1)
    {
        perror("send");
    }
}
