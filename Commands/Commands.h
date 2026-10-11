#pragma once
#ifndef FPANEL_AGENT_COMMANDS_H
#define FPANEL_AGENT_COMMANDS_H
#include <iostream>
#include <Helpers/Helpers.h>
#include <Logger/Logger.h>
#include <Config/Config.h>
#include <Crypto/Crypto.h>
#include <Rapidjson/document.h>
#include <Rapidjson/writer.h>
#include <Rapidjson/stringbuffer.h>
#include <Socket/TcpClient.h>

using namespace rapidjson;

class Commands {
private:
    Helpers *_helpers = new Helpers();
    Logger _logger;
    Config *_config;
    Crypto _crypto;
    bool _debug;
    std::string server_key = "7EDbOEyDQAdg8AjZjRgcX4KOExvmfuWP";
    std::string server_iv = "5AfcicxDnO7H6u7K";
    std::string panel_conf = "/usr/local/fpanel/etc/panel.conf";
    std::string sql_pass_file= "/usr/local/fpanel/etc/.sql_pass";
    std::string config_location = "/usr/local/fpanel/etc/cloud.config.ini";
    std::string system_packages = "/usr/local/fpanel/etc/.sys.packages";

    std::string sysInfo()
    {
        StringBuffer last_s_json;
        Writer<StringBuffer> writer(last_s_json);
        writer.StartObject();
        writer.Key("result");


        // CPU Info
        // Disk Space
        // Ram
        // OS Info
        // IP Addresses
        // Hostname
        writer.StartObject();

        writer.Key("cpu");
        std::string cpu_name = _helpers->GetProcessorName();
        _helpers->trim(cpu_name);
        writer.String(cpu_name.c_str());

        writer.Key("disk");
        writer.StartObject();

        writer.Key("total");
        std::string total_disk = _helpers->GetTotalDiskSpace();
        _helpers->trim(total_disk);
        writer.String(total_disk.c_str());

        writer.Key("used");
        std::string used_disk =_helpers->GetUsedDiskSpace();
        writer.String(used_disk.c_str());
        _helpers->trim(used_disk);
        writer.EndObject();


        writer.Key("hostname");
        std::string hostname = boost::asio::ip::host_name();
        _helpers->trim(hostname);
        writer.String(hostname.c_str());


        writer.Key("memory");
        writer.StartObject();

        writer.Key("total");
        std::string total_memory = _helpers->GetTotalMemory();
        _helpers->trim(total_memory);
        writer.String(total_memory.c_str());

        writer.Key("used");
        std::string used_memory = _helpers->GetUsedMemory();
        _helpers->trim(used_memory);
        writer.String(used_memory.c_str());
        writer.EndObject();

        writer.Key("os");
        std::string os_name = _helpers->CmdOutput("cat /etc/os-release | grep ^PRETTY_NAME | sed -e 's/PRETTY_NAME=//g' -e 's/\"//g'");
        _helpers->trim(os_name);
        writer.String(os_name.c_str());

        writer.Key("default_ip");
        std::string default_ip = _helpers->GetLocalIpAddress();
        _helpers->trim(default_ip);
        writer.String(default_ip.c_str());

        writer.Key("ips");
        writer.StartArray();
        std::vector<std::string> ips = _helpers->IpAddresses();
        for (std::string i : ips) {
            writer.String(i.c_str());
        }
        writer.EndArray();

        writer.EndObject();

        writer.EndObject();
        std::string response_json = last_s_json.GetString();
        last_s_json.Clear();

        return response_json;
    }

    std::string installation(std::string command, const std::string& parameters)
    {
        _helpers->CreateDirectory("/usr/local/fpanel");
        _helpers->CreateDirectory("/usr/local/fpanel/tmp");

        if (_helpers->IsFileExists("/usr/local/fpanel/tmp/installer")) {
            _helpers->RemoveFile("/usr/local/fpanel/tmp/installer");
        }

        _logger.Debug("Download processing...");
        bool download_status = _helpers->downloadFile("repo.fpanel.net", "installer", "/usr/local/fpanel/tmp/installer");
        _logger.Debug("Installer downloaded...");
        // chmod giving
        _helpers->CmdOutput("chmod 777 /usr/local/fpanel/tmp/installer");
        if (download_status) {
            _logger.Debug("Installation starting...");
            _logger.Debug("Running Command: %s", std::string("/usr/local/fpanel/tmp/installer --fast --cloud " + parameters).c_str());
            std::string output = _helpers->CmdOutput("/usr/local/fpanel/tmp/installer --fast --cloud " + parameters + " >>/dev/null;RETVAL=$?;echo $RETVAL");
            _helpers->Write("/usr/local/fpanel/core/bin/debug","test");
            return R"({"result":"success"})";
        }
        return R"({"result":"failed"})";
    }

    std::string getConfigs()
    {
        StringBuffer last_s_json;
        Writer<StringBuffer> writer(last_s_json);
        writer.StartObject();
        writer.Key("result");

        std::ifstream secret_file;
        secret_file.open(panel_conf);
        std::string secret_key;
        std::string secret_iv;
        std::string panel_sql_secret;
        if (secret_file.is_open()) {
            std::string word;
            std::string first_key;

            int i = 0;
            while (secret_file >> word) {
                if (i == 0)
                    secret_key = word;
                if (i == 1)
                    secret_iv = word.substr(0, 16);
                if (i == 2)
                    panel_sql_secret = word;
                i++;
            }
            _helpers->trim(secret_key);
            _helpers->trim(secret_iv);
            _helpers->trim(panel_sql_secret);
            secret_file.close();
        }
        writer.StartObject();
        writer.Key("key");
        writer.String(secret_key.c_str());
        writer.Key("iv");
        writer.String(secret_iv.c_str());
        writer.Key("sql_secret");
        writer.String(panel_sql_secret.c_str());

        writer.Key("system_packages");
        writer.StartObject();
        std::vector<std::string> packages_file_content = _helpers->ReadFile(system_packages);
        for (std::string line : packages_file_content) {
            if(!line.empty()) {
                std::vector<std::string> line_arr;
                _helpers->Split(line_arr, line, "=");
                if(line_arr.size() > 1) {
                    writer.Key(line_arr[0].c_str());
                    writer.String(line_arr[1].c_str());
                }
            }
        }
        writer.EndObject();

        std::string sql_pass;
        std::ifstream s_file;
        s_file.open(sql_pass_file);
        if (s_file.is_open()) {
            std::string word;
            while (s_file >> word) {
                if (!word.empty()) {
                    sql_pass = word;
                }
            }
            s_file.close();
        }
        writer.Key("sql_pass");
        writer.String(sql_pass.c_str());
        writer.EndObject();

        writer.EndObject();
        std::string response_json = last_s_json.GetString();
        last_s_json.Clear();

        return response_json;
    }

    std::string reboot()
    {
        _helpers->CmdOutput("reboot");
        return R"({"result":"success"})";
    }

    std::string initialize()
    {
        StringBuffer last_s_json;
        Writer<StringBuffer> writer(last_s_json);
        writer.StartObject();
        writer.Key("result");

        std::string command = "/usr/local/fpanel/core/bin/adminmng --init";
        std::string cmd_result = _helpers->CmdOutput(command);
        _helpers->trim(cmd_result);
        writer.String(cmd_result.c_str());

        writer.EndObject();
        std::string response_json = last_s_json.GetString();
        last_s_json.Clear();

        return response_json;
    }

    std::string setMain(std::string parameters)
    {
        _helpers->trim(parameters);
        std::string cloud_config_ini = "[socket.server]\n";
        cloud_config_ini += "address=" + parameters;
        _helpers->Write(config_location, cloud_config_ini);
        return R"({"result":"success"})";
    }

    std::string convertToCloud(const std::string& parameters)
    {
        StringBuffer last_s_json;
        Writer<StringBuffer> writer(last_s_json);
        writer.StartObject();
        writer.Key("result");

        std::string command = "/usr/local/fpanel/core/bin/cloudmng --convert-cloud --server-addr=" + parameters;
        std::string cmd_result = _helpers->CmdOutput(command);

        _helpers->trim(cmd_result);
        writer.String(cmd_result.c_str());

        writer.EndObject();
        std::string response_json = last_s_json.GetString();
        last_s_json.Clear();

        return response_json;
    }
public:
    Commands& SetLogger(Logger logger)
    {
        _logger = logger;
        return *this;
    }

    Commands& SetConfig(Config *config)
    {
        _config = config;
        return *this;
    }

    Commands& SetDebug(bool debug)
    {
        _debug = debug;
        return *this;
    }

    std::string Run(std::string command, const std::string& parameters)
    {
        _helpers->trim(command);
        _logger.Debug("Command: %s", command.c_str());
        _logger.Debug("Parameters: %s", parameters.c_str());

        std::string result;

        if (command == "sysinfo") {
            _logger.Debug("SystemInfo getting...");
            result = sysInfo();
        }
        else if (command == "install") {
            _logger.Debug("Installation starting...");
            result = installation(command, parameters);
        }
        else if (command == "get-config") {
            _logger.Debug("Config getting...");
            // including .sql_pass
            result = getConfigs();
        }
        else if (command == "reboot") {
            _logger.Debug("Rebooting...");
            result = reboot();
        }
        else if (command == "set-main") {
            _logger.Debug("Main server setting...");
            result = setMain(parameters);
        }
        else if (command == "init") {
            _logger.Debug("Initialize processing...");
            result = initialize();
        }
        else if (command == "convert-cloud") {
            _logger.Debug("Making a part of Cloud...");
            result = convertToCloud(parameters);
        }

        _logger.Debug("cmd_result: %s", result.c_str());
        return result;
    }
};
#endif //FPANEL_AGENT_COMMANDS_H