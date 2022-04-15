#include <cstdio>
#include <cstdlib>
#include <cstring>

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
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
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
        fprintf(stderr, "chatter-server: failed to bind\n");
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

void Server::addRoom(std::string name)
{
    if (rooms.find(name) != rooms.end())
    {
        Room room(name);
        rooms[name] = room;
    }
}

void Server::addClientToRoom(std::string room, Client client)
{
    if (client.room != room)
    {
        rooms[client.room].removeClient(client);
    }
    rooms[room].addClient(client);
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
        clients[client_fd] = client;
        addClientToRoom("global", client);
        client_pfds.push_back({client_fd, POLLIN, 0});
        printf("New connection from %s on socket %d\n", client.addr.c_str(), client_fd);
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
                        printf("Disconnected %s from socket %d\n", clients[client_fd].addr.c_str(), client_fd);
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
                        std::string send_str = makeSendString(clients[client_fd], msg_buffer);
                        while (send_str.back() != '\0')
                        { // get whole message
                            memset(&msg_buffer[0], 0, MAXDATASIZE);
                            recv(client_fd, &msg_buffer[0], msg_buffer.size(), 0);
                            send_str.append(msg_buffer.cbegin(), msg_buffer.cend());
                        }
                        rooms[clients[client_fd].room].broadCastMsg(client_fd, send_str);
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

std::string Server::makeSendString(Client client, std::vector<char>& buffer)
{
    std::string ret = client.name + "@" + client.addr + "> ";
    ret.append(buffer.cbegin(), buffer.cend());
    return ret;
}