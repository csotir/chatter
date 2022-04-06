#include <poll.h>
#include <vector>
#include "common.h"

#define BACKLOG 10

string make_send_string(const char* addr, vector<char>& buffer)
{
    string ret(addr);
    ret.append("> ");
    ret.append(buffer.cbegin(), buffer.cend());
    return ret;
}

int main()
{
    int server, client, ret;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr;
    socklen_t size;
    int yes = 1;
    char addrBuffer[INET6_ADDRSTRLEN];
    vector<pollfd> clients;
    vector<char> buffer(MAXDATASIZE);

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

    cout << "Waiting for clients...\n";

    while (true)
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
                client = clients[i].fd;
                size = sizeof client_addr;
                if (client == server)
                {
                    client = accept(server, (struct sockaddr*)&client_addr, &size);
                    if (client == -1)
                    {
                        perror("chatter-server: accept");
                    }
                    else
                    {
                        clients.push_back({client, POLLIN, 0});
                        inet_ntop(client_addr.ss_family,
                            get_in_addr((struct sockaddr*)&client_addr),
                            addrBuffer, sizeof addrBuffer);
                        printf("New connection from %s on socket %d\n", addrBuffer, client);
                    }
                }
                else
                {
                    memset(&buffer[0], 0, MAXDATASIZE);
                    int nbytes = recv(client, &buffer[0], buffer.size(), 0);
                    getpeername(client, (struct sockaddr*)&client_addr, &size);
                    if (nbytes <= 0)
                    {
                        if (nbytes == 0)
                        {
                            inet_ntop(client_addr.ss_family,
                                get_in_addr((struct sockaddr*)&client_addr),
                                addrBuffer, sizeof addrBuffer);
                            printf("Disconnected %s from socket %d\n", addrBuffer, client);
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
                        if (buffer[0] != '\r' && buffer[0] != '\n')
                        { // ignore empty messages
                            inet_ntop(client_addr.ss_family,
                                get_in_addr((struct sockaddr*)&client_addr),
                                addrBuffer, sizeof addrBuffer);
                            string sendStr = make_send_string(addrBuffer, buffer);
                            while (sendStr.back() != '\0')
                            { // get whole message
                                memset(&buffer[0], 0, MAXDATASIZE);
                                recv(client, &buffer[0], buffer.size(), 0);
                                sendStr.append(buffer.cbegin(), buffer.cend());
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

    return 0;
}
