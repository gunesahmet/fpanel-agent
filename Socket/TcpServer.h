#pragma once
#ifndef FPANEL_AGENT_TCPSERVER_H
#define FPANEL_AGENT_TCPSERVER_H
#include <execinfo.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <pwd.h>
#include <shadow.h>
#include <crypt.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <thread>
#include <vector>
#include <list>


#include <unistd.h>
#include <libgen.h>

#include <functional>

#include <errno.h>
#include <strings.h>
#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.
#include <boost/lexical_cast.hpp>

#include <Logger/Logger.h>
#include <Helpers/Helpers.h>
#include <Config/Config.h>
#include <Crypto/Crypto.h>
#include <Commands/Commands.h>

#include <Rapidjson/document.h>
#include <Rapidjson/writer.h>
#include <Rapidjson/stringbuffer.h>

using namespace rapidjson;
class TcpServer {
private:
    std::string _host;
    int _port;
    Logger _logger;
    Config *_config;
    Crypto _crypto;
    Helpers _helpers;
    Commands *_commands;

    static void errorHandler(int sig) {
        size_t nStackTraces = 20;
        void *array[nStackTraces];
        size_t size;
        size = backtrace(array, nStackTraces);
        backtrace_symbols_fd(array, size, STDERR_FILENO);
        exit(0);
    };

    char* getipaddr(int sock)
    {
        sockaddr_in SockAddr;
        struct sockaddr_in addr;
        socklen_t addr_size = sizeof(struct sockaddr_in);
        int res = getpeername(sock, (struct sockaddr *)&addr, &addr_size);
        char *client_ip = new char[20];
        strcpy(client_ip, inet_ntoa(addr.sin_addr));
        int port = ntohs(addr.sin_port);
        _logger.Debug("[IP: %s Connected On PORT: %d]", client_ip, port);
        return client_ip;
    }

    int connectionHandler(void *socket_desc)
    {
        int clientSock = *(int*)socket_desc;
        std::string rcv;
        int bytesReceived = 0;
        const int MAX_BUF_LENGTH = 1024;
        char buffer[MAX_BUF_LENGTH];
        char err[100];

        while (true) {
            bytesReceived = recv(clientSock, buffer, sizeof(buffer), 0);
            if (bytesReceived < 0) {
                _logger.Debug("Receiving data failed");
                break;
            }
            else if (bytesReceived == 0) {
                _logger.Debug("Client disconnected");
                break;
            }
            else {
                rcv.append(buffer, bytesReceived);
                if (bytesReceived < MAX_BUF_LENGTH) {
                    break;
                }
            }
        }
        _helpers.trim(rcv);
        std::string command;
        bool with_auth = true;
        bool validated = false;
        //if (_config->getValue("secret.key").length() > 0) {
        //	command = _crypto.Decrypt(rcv, _config->getValue("secret.key"), _config->getValue("secret.iv"));
        //	with_auth = false;
        //	validated = true;
        //}
        //else {
        //	command = _crypto.Decrypt(rcv, _config->getValue("socket.key"), _config->getValue("socket.iv"));
        //}
        command = _crypto.Decrypt(rcv, _config->getValue("socket.key"), _config->getValue("socket.iv"));

        if (command.length() == 0) {
            _logger.Error("Command length is 0");
            close(clientSock);
            return 1;
        }

        Document d;
        const char *json = command.c_str();
        if (!d.Parse(json).HasParseError()) {
            if (d.HasMember("job")) {
                if (with_auth && d.HasMember("username") && d.HasMember("password")) {
                    if (validateUser(d["username"].GetString(), d["password"].GetString()) == 0) {
                        validated = true;
                    }
                }
                if (validated) {
                    std::string job = d["job"].GetString();
                    if (std::string(job).compare("exec") == 0) {
                        if (d.HasMember("command")) {
                            _logger.Debug("Executing command");
                            initializeRequest(d["command"].GetString(), d["parameters"].GetString(), clientSock);
                        }
                    }
                }
                else {
                    _logger.Debug("Authentication failed");
                }
            }
        }
        else {
            _logger.Error("Invalid command. Received: ", command.c_str());
        }


        _logger.Debug("=============================");
        _logger.Debug("=============================");
        _logger.Debug("=============================");
        rcv.clear();
        close(clientSock);
        _logger.Debug("Socket client closed");

        return 0;
    }

    void initializeRequest(std::string command, std::string parameters, int clientSock)
    {
        std::string response = _commands->Run(command, parameters);
        Send(clientSock, response);
    }

    int validateUser(const char *username, const char *password)
    {
        std::string buffer;
        struct passwd* passwd_entry = getpwnam(username);
        if (!passwd_entry) {
            _logger.Debug("User '%s' doesn't exist", username);
            return 1;
        }

        if (0 != strcmp(passwd_entry->pw_passwd, "x")) {
            return strcmp(passwd_entry->pw_passwd, crypt(password, passwd_entry->pw_passwd));
        }
        else {
            // password is in shadow file
            struct spwd* shadow_entry = getspnam(username);
            if (!shadow_entry) {
                _logger.Debug("Failed to read shadow entry for user %s", username);
                return 1;
            }
            return strcmp(shadow_entry->sp_pwdp, crypt(password, shadow_entry->sp_pwdp));
        }
    }

public:
    TcpServer& SetLogger(Logger logger)
    {
        _logger = logger;
        return *this;
    }

    TcpServer& SetConfig(Config *config)
    {
        _config = config;
        return *this;
    }

    TcpServer& SetHost(std::string host)
    {
        _host = host;
        return *this;
    }

    TcpServer& SetCommands(Commands *commands)
    {
        _commands = commands;
        return *this;
    }

    TcpServer& SetPort(int port)
    {
        _port = port;
        return *this;
    }

    void Send(int client_sock, std::string &response)
    {
        std::string crypted_response = _crypto.Encrypt(response, _config->getValue("socket.key"), _config->getValue("socket.iv")) + "\n";
        if(send(client_sock, crypted_response.c_str(), strlen(crypted_response.c_str()), 0) < 0) {
            _logger.Error("Send failed to client");
        }
    }

    void Start()
    {
        struct sockaddr_in sa_server;
        int listen_socket;

        struct sockaddr client_address;
        socklen_t size_of_client_address = sizeof(client_address);
        listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (-1 == listen_socket) {
            _logger.Error("Could not create socket");
            exit(EXIT_FAILURE);
        }

        int enable = 1;
        if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            _logger.Error("setsockopt(SO_REUSEADDR) failed");
        }

        uint16_t port = uint16_t(_port);

        sa_server.sin_family = AF_INET;
        sa_server.sin_addr.s_addr = inet_addr(_host.c_str());
        sa_server.sin_port = htons(port);

        if (-1 == bind(listen_socket, (struct sockaddr *) &sa_server, sizeof (sa_server))) {
            _logger.Error("Error : Bind Failed");
            close(listen_socket);
            exit(EXIT_FAILURE);
        }
        if (-1 == listen(listen_socket, 1000)) {
            _logger.Error("Error : Listen Failed");
            close(listen_socket);
            exit(EXIT_FAILURE);
        }

        while(true) {
            try {
                int new_client = accept(listen_socket, &client_address, &size_of_client_address);
                getipaddr(new_client);
                std::thread t(&TcpServer::connectionHandler, this, (void*) &new_client);
                t.detach();
            }
            catch (const std::exception& e) {
                _logger.Error("Error : Socket accept. Exception: %s", std::string(e.what()).c_str());
                continue;
            }
        }
    }
};
#endif //FPANEL_AGENT_TCPSERVER_H