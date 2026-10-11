#pragma once
#ifndef FPANEL_AGENT_TCP_CLIENT_H
#define FPANEL_AGENT_TCP_CLIENT_H
#ifdef WIN32
#elif __linux__
#include <netdb.h>
#include <arpa/inet.h>
#endif
#include <stdio.h>
#include <Logger/Logger.h>
#include <Helpers/Helpers.h>
#include <Config/Config.h>
#include <Crypto/Crypto.h>
class TcpClient {
private:
    Crypto _crypto;
    Logger _logger;
    Config *_config;
    Helpers _helpers;
    int sock_server = 0;
    struct sockaddr_in address;
    struct sockaddr_in server_address;
    std::string _s_addr;
    std::string _response;
    int _s_port;

public:
    void SetLogger(Logger logger)
    {
        _logger = logger;
    }

    void SetConfig(Config *config)
    {
        _config = config;
    }

    void SetServer(std::string s_addr, int s_port)
    {
        _s_addr = s_addr;
        _s_port = s_port;
    }

    bool Send(std::string command)
    {
        bool status = true;
        std::string request = command;
        request += "\n";

        if(send(sock_server, request.c_str(), strlen(request.c_str()), 0) < 0) {
            _logger.Error("Send message failed to server");
            status = false;
        }
        else {
            _logger.Debug("Send successful message");
        }

        return status;
    }

    void Read()
    {
        int n = 1, total = 0, found = 0;
        char c;
        char temp[1024*1024];

        while (!found) {
            n = recv(sock_server, &temp[total], sizeof(temp) - total - 1, 0);
            if (n == -1) {
                break;
            }
            total += n;
            temp[total] = '\0';
            found = (strchr(temp, '\n') != 0);
        }
        _response = temp;
    }

    std::string Response()
    {
//    std::string response = _crypto.Decrypt(_response, _config->getValue("secret.key"), _config->getValue("secret.iv"));
        std::string response = _response;
        _response = "";
        return response;
    }

    bool Close()
    {
        close(sock_server);
    }

    bool Connect(std::string request)
    {
        int err;
        struct addrinfo hints = {}, *addrs;
        char port_str[16] = {};

        hints.ai_family = AF_INET; // Since your original code was using sockaddr_in and
        // PF_INET, I'm using AF_INET here to match.  Use
        // AF_UNSPEC instead if you want to allow getaddrinfo()
        // to find both IPv4 and IPv6 addresses for the hostname.
        // Just make sure the rest of your code is equally family-
        // agnostic when dealing with the IP addresses associated
        // with this connection. For instance, make sure any uses
        // of sockaddr_in are changed to sockaddr_storage,
        // and pay attention to its ss_family field, etc...
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        sprintf(port_str, "%d", _s_port);

        err = getaddrinfo(_s_addr.c_str(), port_str, &hints, &addrs);
        if (err != 0) {
            std::cout << std::string(_s_addr) << ": " << gai_strerror(err) << std::endl;
            return false;
        }

        for(struct addrinfo *addr = addrs; addr != NULL; addr = addr->ai_next) {
            sock_server = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
            if (sock_server == -1) {
                err = errno;
                break; // if using AF_UNSPEC above instead of AF_INET/6 specifically,
                // replace this 'break' with 'continue' instead, as the 'ai_family'
                // may be different on the next iteration...
            }

            if (connect(sock_server, addr->ai_addr, addr->ai_addrlen) == 0)
                break;

            err = errno;

            close(sock_server);
            sock_server = -1;
        }

        freeaddrinfo(addrs);

        if (sock_server == -1) {
            std::cout << std::string(_s_addr) << ": " << strerror(err) << std::endl;
            return false;
        }

        return true;
    }
};
#endif //FPANEL_AGENT_TCP_CLIENT_H