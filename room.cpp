#include <cstdio>

#include <sys/socket.h>

#include "room.h"

Room::Room(std::string room_name) : name(room_name) { }

void Room::addClient(Client client)
{
    clients.insert(client.fd);
    std::string msg = client.name + "@" + client.addr + " has joined the room!\n";
    broadCastMsg(client.fd, msg);
}

void Room::removeClient(Client client)
{
    clients.erase(client.fd);
    std::string msg = client.name + "@" + client.addr + " has left the room!\n";
    broadCastMsg(client.fd, msg);
}

void Room::broadCastMsg(int sender, std::string msg)
{
    for (auto dest : clients)
    { // send to clients
        if (dest != sender)
        { // except sender and server
            if (send(dest, msg.c_str(), msg.size(), 0) == -1)
            {
                perror("send");
            }
        }
    }
}