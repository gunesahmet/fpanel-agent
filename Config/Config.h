#pragma once
#ifndef FPANEL_AGENT_CONFIG_H
#define FPANEL_AGENT_CONFIG_H

#include <iostream>
#include <map>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>

#include <Logger/Logger.h>
#include <Crypto/Crypto.h>
#include <Helpers/Helpers.h>


class Config {
private:
    Logger _logger;
    std::map<std::string, std::string> configurations;
    std::mutex conf_mutex;
    Helpers _helpers;
    std::string panel_conf = "/usr/local/fpanel/etc/panel.conf";

public:
    Config()
    {
        configurations = std::map<std::string, std::string>();
    }

    ~Config()
    {
        configurations.clear();
    }

    void setLogger(Logger logger)
    {
        _logger = logger;
    }

    void init()
    {
        Crypto crypto;
        std::lock_guard<std::mutex> lockGuard(conf_mutex);

        std::ifstream file;
        file.open(panel_conf);

        configurations.empty();
        configurations.clear();

        std::string secret_key;
        std::string secret_iv;

        if (file.is_open()) {
            std::string word;
            std::string first_key;

            int i = 0;
            while (file >> word) {
                if (i == 0)
                    secret_key = word;
                if (i == 1)
                    secret_iv = word.substr(0, 16);
                i++;
            }
            _helpers.trim(secret_key);
            _helpers.trim(secret_iv);
            file.close();
        }

        configurations.insert({ "secret.key", secret_key });
        configurations.insert({ "secret.iv", secret_iv });

        std::string socket_key = "7EDbOEyDQAdg8AjZjRgcX4KOExvmfuWP";
        std::string socket_iv = "5AfcicxDnO7H6u7K";

        configurations.insert({ "socket.key", socket_key });
        configurations.insert({ "socket.iv", socket_iv });

        // TCP Sockets
        std::string socket_server_address = "0.0.0.0";
        std::string socket_server_port = "8887";
        std::string file_socket_server_port = "8888";
        std::string panel_dir = "/usr/local/fpanel/";


        configurations.insert({ "socket.server.address", socket_server_address });
        configurations.insert({ "socket.server.port", socket_server_port });
        configurations.insert({ "file.socket.server.address", socket_server_address });
        configurations.insert({ "file.socket.server.port", file_socket_server_port });
        configurations.insert({ "panel_dir", panel_dir });
        configurations.insert({ "root_bin_path", panel_dir + "core/bin/" });
        configurations.insert({ "root_sbin_path", panel_dir + "core/sbin/" });
        configurations.insert({ "httpd_dir", panel_dir + "httpd/" });
        configurations.insert({ "htdocs_dir", panel_dir + "httpd/htdocs/" });
    }

    std::map<std::string, std::string> getConfigs()
    {
        std::lock_guard<std::mutex> lockGuard(conf_mutex);
        return configurations;
    }

    std::string getValue(std::string key)
    {
        std::lock_guard<std::mutex> lockGuard(conf_mutex);
        return configurations.find(key)->second;
    }
};
#endif//FPANEL_AGENT_CONFIG_H