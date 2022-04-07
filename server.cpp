#include "server.h"

vector<char> msg_buffer(MAXDATASIZE);
char addr_buffer[INET6_ADDRSTRLEN];
struct sockaddr_storage client_addr;
socklen_t addr_size = sizeof client_addr;

int Server::makeConnection()
{
    struct addrinfo hints, *servinfo, *p;
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

    clients.push_back({server, POLLIN, 0});

    return 0;
}

void Server::pollClients()
{
    int poll_count = poll(&clients[0], (nfds_t)clients.size(), -1);
    if (poll_count == -1)
    {
        perror("poll");
        exit(1);
    }

    for (int i = 0; i < clients.size();)
    {
        if (clients[i].revents & POLLIN)
        {
            int client = clients[i].fd;
            if (client == server)
            {
                connectClient();
            }
            else
            {
                memset(&msg_buffer[0], 0, MAXDATASIZE);
                int nbytes = recv(client, &msg_buffer[0], msg_buffer.size(), 0);
                getpeername(client, (struct sockaddr*)&client_addr, &addr_size);
                if (nbytes <= 0)
                {
                    if (nbytes == 0)
                    {
                        inet_ntop(client_addr.ss_family,
                            get_in_addr((struct sockaddr*)&client_addr),
                            addr_buffer, sizeof addr_buffer);
                        printf("Disconnected %s from socket %d\n", addr_buffer, client);
                    }
                    else
                    {
                        perror("recv");
                    }
                    close(client);
                    clients.erase(clients.begin() + i);
                    continue;
                }
                else
                {
                    if (msg_buffer[0] != '\r' && msg_buffer[0] != '\n')
                    { // ignore empty messages
                        inet_ntop(client_addr.ss_family,
                            get_in_addr((struct sockaddr*)&client_addr),
                            addr_buffer, sizeof addr_buffer);
                        string sendStr = makeSendString(addr_buffer, msg_buffer);
                        while (sendStr.back() != '\0')
                        { // get whole message
                            memset(&msg_buffer[0], 0, MAXDATASIZE);
                            recv(client, &msg_buffer[0], msg_buffer.size(), 0);
                            sendStr.append(msg_buffer.cbegin(), msg_buffer.cend());
                        }
                        for (auto itr2 = clients.begin(); itr2 != clients.end(); itr2++)
                        { // send to clients
                            int dest = itr2->fd;
                            if (dest != client && dest != server)
                            { // except sender and server
                                if (send(dest, sendStr.c_str(), sendStr.size(), 0) == -1)
                                {
                                    perror("send");
                                }
                            }
                        }
                    }
                }
            }
        }
        i++;
    }
}

void Server::connectClient()
{
    int client = accept(server, (struct sockaddr*)&client_addr, &addr_size);
    if (client == -1)
    {
        perror("chatter-server: accept");
    }
    else
    {
        clients.push_back({client, POLLIN, 0});
        inet_ntop(client_addr.ss_family,
            get_in_addr((struct sockaddr*)&client_addr),
            addr_buffer, sizeof addr_buffer);
        printf("New connection from %s on socket %d\n", addr_buffer, client);
    }
}

string Server::makeSendString(const char* addr, vector<char>& buffer)
{
    string ret(addr);
    ret.append("> ");
    ret.append(buffer.cbegin(), buffer.cend());
    return ret;
}
