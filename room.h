#ifndef _ROOM_H
#define _ROOM_H

#include <string>
#include <unordered_set>

#include "client.h"

class Room
{
    public:
        Room() = default;
        Room(const std::string& room_name);
        void addClient(const Client& client);
        void removeClient(const Client& client);
        void broadCastMsg(int sender, const std::string& msg);
        const std::unordered_set<int>& getClients() { return clients; }
    private:
        std::string name;
        std::unordered_set<int> clients;
};

#endif
