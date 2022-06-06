#ifndef CHATTER_COMMAND_HANDLER_H_
#define CHATTER_COMMAND_HANDLER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "client.h"

namespace chatter {

class Server;

enum class Command
{
    NAME,
    WHO,
    ROOMS,
    JOIN,
    LEAVE,
    TELL,
    RANDOM,
    HELP,
};

const std::unordered_map<std::string, Command> Commands
{
    {"name", Command::NAME},
    {"who", Command::WHO},
    {"rooms", Command::ROOMS},
    {"join", Command::JOIN},
    {"leave", Command::LEAVE},
    {"tell", Command::TELL},
    {"random", Command::RANDOM},
    {"help", Command::HELP},
};

const std::vector<std::string> HelpText
{
    "/name <name>            : Change your display name.",
    "/who                    : List users in current room.",
    "/who <room>             : List users in specified room.",
    "/rooms                  : List rooms.",
    "/join <room>            : Join/create the specified room.",
    "/join <room> <password> : Join/create a password protected room.",
    "/leave                  : Leave the current room.",
    "/tell <#> <message>     : Send a direct message to the specified user #.",
    "/random                 : Roll a random number from 0 to 99.",
    "/help                   : Display available commands.",
};

class CommandHandler
{
    public:
        CommandHandler(Server& server) : server_(&server) { };
        void ParseCommand(Client& client, std::string& command);
    private:
        std::string GetToken(std::string& message) const;
        bool SanitizeString(std::string& str, bool lower = false) const;
        void Who(const Client& client, std::string& message) const;
        void Name(Client& client, std::string& message);
        void Rooms(const Client& client) const;
        void Join(Client& client, std::string& message);
        void Leave(Client& client);
        void Tell(const Client& client, std::string& message) const;
        void Random(const Client& client) const;
        void Help(const Client& client) const;
        Server* server_;
};

} // namespace chatter

#endif //CHATTER_COMMAND_HANDLER_H