#pragma once
#ifndef FPANEL_AGENT_LOGGER_H
#define FPANEL_AGENT_LOGGER_H

#include <iostream>
#include <plog/Log.h>
#include <plog/Appenders/ConsoleAppender.h>

class Logger {
    enum {
        SecondLog = 1,
        MaxLogFileSize = 104857600 // 100MB
    };
public:
    bool debug = false;
    template<typename ... Args>
    void Info(const std::string &format, Args ... args) {
        int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
        if (size_s <= 0) {
            throw std::runtime_error("Error during formatting.");
        }
        auto size = static_cast<size_t>(size_s);
        std::unique_ptr<char[]> buf(new char[size]);
        std::snprintf(buf.get(), size, format.c_str(), args ...);
        std::string message = std::string(buf.get(), buf.get() + size - 1);

        if (debug) {
            std::cout << "INFO: " << message << std::endl;
        }
        LOGI << message;
    }

    template<typename ... Args>
    void Error(const std::string &format, Args ... args) {
        int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
        if (size_s <= 0) {
            throw std::runtime_error("Error during formatting.");
        }
        auto size = static_cast<size_t>(size_s);
        std::unique_ptr<char[]> buf(new char[size]);
        std::snprintf(buf.get(), size, format.c_str(), args ...);
        std::string message = std::string(buf.get(), buf.get() + size - 1);
        if (debug) {
            std::cout << "ERROR: " << message << std::endl;
        }
        LOGE << message;
    }

    template<typename ... Args>
    void Debug(const std::string &format, Args ... args) {
        int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
        if (size_s <= 0) {
            throw std::runtime_error("Error during formatting.");
        }
        auto size = static_cast<size_t>(size_s);
        std::unique_ptr<char[]> buf(new char[size]);
        std::snprintf(buf.get(), size, format.c_str(), args ...);
        std::string message = std::string(buf.get(), buf.get() + size - 1);
        if (debug) {
            std::cout << "DEBUG: " << message << std::endl;
            LOGD << message;
        }
    }
};
#endif //FPANEL_AGENT_LOGGER_H