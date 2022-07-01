#include "command_handler.h"

#include "colors.h"
#include "server.h"

namespace chatter {

std::string CommandHandler::GetToken(std::string& message) const
{
    std::string ret;
    int pos = 0;
    while (ret.empty() && (pos = message.find(' ')) != std::string::npos)
    {
        ret += message.substr(0, pos);
        message.erase(0, pos + 1);
    }
    if (ret.empty())
    {
        ret = message;
        message.erase(0, std::string::npos);
    }
    return ret;
}

bool CommandHandler::SanitizeString(std::string& str, bool lower) const
{
    for (int i = 0; i < str.size();)
    {
        if (str[i] < 32)
        {
            str.erase(str.begin() + i);
            continue;
        }
        if (!isalpha(str[i]))
        {
            str.erase(0, std::string::npos);
            return false;
        }
        if (lower)
        {
            str[i] = tolower(str[i]);
        }
        ++i;
    }
    return true;
}

void CommandHandler::ParseCommand(Client& client, std::string& message)
{
    std::string command = GetToken(message);
    if (!SanitizeString(command, true))
    {
        server_->SendToClient(client.fd, "", chatter::colors::Red, "Invalid command.\r\n");
        return;
    }

    /* COMMANDS */
    if (chatter::Commands.find(command) != chatter::Commands.end())
    {
        switch (chatter::Commands.at(command))
        {
            case Command::NAME:
            {
                Name(client, message);
                break;
            }
            case Command::WHO:
            {
                Who(client, message);
                break;
            }
            case Command::ROOMS:
            {
                Rooms(client);
                break;
            }
            case Command::JOIN:
            {
                Join(client, message);
                break;
            }
            case Command::LEAVE:
            {
                Leave(client);
                break;
            }
            case Command::TELL:
            {
                Tell(client, message);
                break;
            }
            case Command::RANDOM:
            {
                Random(client);
                break;
            }
            case Command::COLOR:
            {
                Color(client);
                break;
            }
            case Command::HELP:
            {
                Help(client);
                break;
            }
        }
    }
    else
    {
        server_->SendToClient(client.fd, "", chatter::colors::Red, "Unknown command.\r\n");
    }
}

void CommandHandler::Who(const Client& client, std::string& message) const
{
    std::string room_name = GetToken(message);
    std::string out;
    if (!SanitizeString(room_name, true))
    {
        server_->SendToClient(client.fd, "", chatter::colors::Red, "Invalid room name.\r\n");
        return;
    }
    if (!room_name.empty())
    {
        if (server_->rooms_.find(room_name) == server_->rooms_.end())
        {
            server_->SendToClient(client.fd, "", chatter::colors::Red, "Room \"" + room_name + "\" doesn't exist!\r\n");
            return;
        }
        else
        {
            room_name = room_name;
        }
    }
    else
    {
        room_name = client.room_name;
    }
    out += "Members of room \"" + room_name + "\":\r\n";
    for (auto member_fd : server_->rooms_.at(room_name).GetMembers())
    {
        out += "[" + std::to_string(member_fd) + "]" + server_->clients_.at(member_fd).name;
        if (member_fd == client.fd)
        {
            out += " (you)";
        }
        out += "\r\n";
    }
    server_->SendToClient(client.fd, "", chatter::colors::None, out);
}

void CommandHandler::Name(Client& client, std::string& message)
{
    std::string new_name = GetToken(message);
    if (!SanitizeString(new_name))
    {
        server_->SendToClient(client.fd, "", chatter::colors::Red, "Invalid name.\r\n");
        return;
    }
    if (!new_name.empty())
    {
        std::string out = "[" + std::to_string(client.fd) + "]" + client.name + " is now known as ";
        client.name = new_name;
        server_->SendToClient(client.fd, "", chatter::colors::None, "Your new name is " + client.name + ".\r\n");
        out += client.name + ".\r\n";
        server_->rooms_.at(client.room_name).BroadCastMessage(client.fd, chatter::colors::Yellow, out);
    }
    else
    {
        server_->SendToClient(client.fd, "", chatter::colors::Red, "Please specify a new name.\r\n");
    }
}

void CommandHandler::Rooms(const Client& client) const
{
    std::string out = "Rooms (members):\r\n";
    for (const auto& room : server_->rooms_)
    {
        out += room.first + " (" + std::to_string(room.second.GetMembers().size()) + ")\r\n";
    }
    server_->SendToClient(client.fd, "", chatter::colors::None, out);
}

void CommandHandler::Join(Client& client, std::string& message)
{
    std::string new_room = GetToken(message);
    if (!SanitizeString(new_room, true))
    {
        server_->SendToClient(client.fd, "", chatter::colors::Red, "Invalid room name.\r\n");
        return;
    }
    if (!new_room.empty())
    {
        if (client.room_name == new_room)
        {
            server_->SendToClient(client.fd, "", chatter::colors::Red, "You are already in that room.\r\n");
        }
        else
        {
            std::string password = GetToken(message);
            if (new_room == "global")
            {
                password.clear();
            }
            if (!password.empty())
            {
                password.erase(password.size() - 2, std::string::npos); // remove newline
            }
            server_->AddClientToRoom(client, new_room, password);
        }
    }
    else
    {
        server_->SendToClient(client.fd, "", chatter::colors::Red, "Please specify a room to join.\r\n");
    }
}

void CommandHandler::Leave(Client& client)
{
    if (client.room_name == "global")
    {
        server_->SendToClient(client.fd, "", chatter::colors::Red, "You are already in the global room.\r\n");
    }
    else
    {
        server_->AddClientToRoom(client, "global", "");
    }
}

void CommandHandler::Tell(const Client& client, std::string& message) const
{
    int dest_fd;
    try
    {
        dest_fd = std::stoi(GetToken(message));
    }
    catch (...)
    {
        server_->SendToClient(client.fd, "", chatter::colors::Red, "Please enter a valid recipient number.\r\n");
        return;
    }
    if (server_->clients_.find(dest_fd) == server_->clients_.end())
    {
        server_->SendToClient(client.fd, "", chatter::colors::Red, "User #" + std::to_string(dest_fd) + " does not exist.\r\n");
    }
    else
    {
        if (!message.empty())
        {
            Client& dest = server_->clients_.at(dest_fd);
            std::string timestamp = server_->GetTimestamp();
            server_->SendToClient(client.fd, timestamp, chatter::colors::Magenta, ">>[" +
                std::to_string(dest_fd) + "]" + dest.name + " : " + message);
            server_->SendToClient(dest.fd, timestamp, chatter::colors::Magenta, "[" +
                std::to_string(client.fd) + "]" + client.name + ">> " + message);
        }
    }
}

void CommandHandler::Random(const Client& client) const
{
    std::string out = "[" + std::to_string(client.fd) + "]Random! " + client.name +
        " rolled " + std::to_string(rand() % 100) + ".\r\n" + chatter::colors::Reset;
    server_->rooms_.at(client.room_name).BroadCastMessage(server_->server_fd_, chatter::colors::Yellow, out);
}

void CommandHandler::Color(Client& client)
{
    client.color = !client.color;
    std::string color_display = client.color ? "enabled" : "disabled";
    server_->SendToClient(client.fd, "", chatter::colors::None, "Color is now " + color_display + ".\r\n");
}

void CommandHandler::Help(const Client& client) const
{
    std::string out;
    for (const auto& help_text : chatter::HelpText)
    {
        out += help_text + "\r\n";
    }
    server_->SendToClient(client.fd, "", chatter::colors::None, out);
}

} // namespace chatter
