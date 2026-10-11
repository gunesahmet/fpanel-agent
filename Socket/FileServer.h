#pragma once
#ifndef FPANEL_AGENT_FILE_SERVER_H
#define FPANEL_AGENT_FILE_SERVER_H
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
#include <algorithm>
#include <vector>

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

#include <Rapidjson/document.h>
#include <Rapidjson/writer.h>
#include <Rapidjson/stringbuffer.h>
#include <list>
#include <fstream>
using namespace rapidjson;
class FileServer {
private:
    std::string _host;
    int _port;
    Logger _logger;
    Config *_config;
    Crypto _crypto;
    Helpers _helpers;
    std::string panel_conf = "/usr/local/fpanel/etc/panel.conf";


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
        _logger.Debug("IP: %s Connected On PORT: %d", client_ip, port);
        return client_ip;
    }

    void readFile(int client_sock, std::string file_name, std::string file_path, std::string file_type)
    {
        std::string rcv;
        int bytes_received = 0;
        const int MAX_BUF_LENGTH = 4096; // 32KB
        char buffer[MAX_BUF_LENGTH];
        char err[100];
        _helpers.ReplaceWithString(file_path, "c:/fpanel/", "/usr/local/fpanel/");

        std::string base_directory = file_path;
        _helpers.ReplaceWithString(base_directory, file_name, "");
        if(!_helpers.IsDirExist(base_directory)) {
            _helpers.CreateDirectory(base_directory);
        }

        std::string write_file_path = base_directory + "/" + file_name;
        std::ofstream fout;
        fout.open(write_file_path.c_str(), std::ios::binary | std::ios::out);

        while (true) {
            bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);
            std::cout << "bytes_received: " << bytes_received << std::endl;
            if (bytes_received < 0) {
                _logger.Debug("Receiving data failed");
                break;
            }
            else if (bytes_received == 0) {
                _logger.Debug("Client disconnected");
                break;
            }
            else {
                fout.write(buffer, bytes_received);
                if (bytes_received == 0) {
                    break;
                }
            }
        }
        fout.close();
        _logger.Debug("File wrote");
    }

    int connectionHandler(void *socket_desc)
    {
        int client_sock = *(int*)socket_desc;

        std::string rcv;
        int bytes_received = 0;
        const int MAX_BUF_LENGTH = 4096;
        char buffer[MAX_BUF_LENGTH];
        char err[100];

        while (true) {
            bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);
            if (bytes_received < 0) {
                _logger.Debug("Receiving data failed");
                break;
            }
            else if (bytes_received == 0) {
                _logger.Debug("Client disconnected");
                break;
            }
            else {
                rcv.append(buffer, bytes_received);
                if (bytes_received < MAX_BUF_LENGTH) {
                    break;
                }
            }
        }
        std::string response;
        _helpers.trim(rcv);
        std::string command;
        bool with_auth = true;
        bool validated = false;
        _logger.Debug("Received data: %s", rcv.c_str());

        std::string secret_key, secret_iv;
        if (_helpers.IsFileExists(panel_conf)) {
            secret_key = _config->getValue("secret.key");
            secret_iv = _config->getValue("secret.iv");
        }
        else {
            secret_key = _config->getValue("socket.key");
            secret_iv = _config->getValue("socket.iv");
        }
        command = _crypto.Decrypt(rcv, secret_key, secret_iv);

//        if(_helpers.IsFileExists(panel_conf)) {
//            command = _crypto.Decrypt(rcv, _config->getValue("secret.key"), _config->getValue("secret.iv"));
//        } else {
//            command = _crypto.Decrypt(rcv, _config->getValue("socket.key"), _config->getValue("socket.iv"));
//        }
        _logger.Debug("Received Command: %s", command.c_str());

        if (command.length() == 0) {
            _logger.Debug("Command length is 0");
            response = R"({"result":"failed", "message": "Invalid command"})";
            Send(client_sock, response);
            close(client_sock);
            return 1;
        }

        Document d;
        const char *json = command.c_str();
        if (!d.Parse(json).HasParseError()) {
            if (d.HasMember("job")) {
                if (d.HasMember("username") && d.HasMember("password")) {
                    std::string username = d["username"].GetString();
                    std::string password = d["password"].GetString();
                    if((username.compare(secret_key) == 0)  && (password.compare(secret_iv) == 0)) {
//                    if (validateUser(d["username"].GetString(), d["password"].GetString())) {
                        std::string job = d["job"].GetString();
                        if (std::string(job).compare("file_transfer") == 0) {
                            std::string file_name = d["file_name"].GetString();
                            std::string file_path = d["file_path"].GetString();
                            std::string file_type = d["file_type"].GetString();
                            response = R"({"result":"success"})";
                            _logger.Debug("Response: %s", response.c_str());
                            Send(client_sock, response);
                            readFile(client_sock, d["file_name"].GetString(), d["file_path"].GetString(), d["file_type"].GetString());
                        }
                    }
                    else {
                        response = R"({"result":"failed", "message": "Invalid credentials"})";
                        _logger.Debug("Invalid credential. Received: %s", command.c_str());
                        Send(client_sock, response);
                    }
                }
            }
            else {
                response = R"({"result":"failed", "message": "Missing parameters"})";
                _logger.Debug("Missing parameters. Received: %s", command.c_str());
                Send(client_sock, response);
            }

        }
        else {
            _logger.Debug("Invalid command. Received: %s", command.c_str());
            response = R"({"result":"failed", "message": "Invalid command"})";
            Send(client_sock, response);
        }

        _logger.Debug("Socket closing");
        _logger.Debug("=============================");
        rcv.clear();
        close(client_sock);
        return 0;
    }

    bool validateUser(std::string username, std::string password)
    {
        if((username.compare(_config->getValue("socket.key")) != 0)  || (password.compare(_config->getValue("socket.iv")) != 0)){
            return false;
        }
        return true;
    }

public:
    FileServer& SetLogger(Logger logger)
    {
        _logger = logger;
        return *this;
    }

    FileServer& SetConfig(Config *config)
    {
        _config = config;
        return *this;
    }

    FileServer& SetHost(std::string host)
    {
        _host = host;
        return *this;
    }

    FileServer& SetPort(int port)
    {
        _port = port;
        return *this;
    }

    void Send(int client_sock, std::string &response)
    {
        std::string crypted_response;
        if(_helpers.IsFileExists(panel_conf)) {
            crypted_response = _crypto.Encrypt(response, _config->getValue("secret.key"), _config->getValue("secret.iv")) + "\n";
        } else {
            crypted_response = _crypto.Encrypt(response, _config->getValue("socket.key"), _config->getValue("socket.iv")) + "\n";
        }

        if(send(client_sock, crypted_response.c_str(), strlen(crypted_response.c_str()), 0) < 0) {
            _logger.Error("Send failed to client");
        }
        else {
            _logger.Debug("Response sent");
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
                std::thread t(&FileServer::connectionHandler, this, (void*) &new_client);
                t.detach();
            }
            catch (const std::exception& e) {
                _logger.Error("Error : Socket accept. Exception: %s", std::string(e.what()).c_str());
                continue;
            }
        }
    }
};
#endif//FPANEL_AGENT_FILE_SERVER_H