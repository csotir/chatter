#ifndef _ROOM_H
#define _ROOM_H

#include <string>
#include <unordered_set>

#include "client.h"

class Room
{
    public:
        Room() = default;
        Room(std::string room_name);
        void addClient(Client client);
        void removeClient(Client client);
        void broadCastMsg(int sender, std::string msg);
    private:
        std::string name;
        std::unordered_set<int> clients;
};

#endif