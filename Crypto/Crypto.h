#pragma once
#ifndef FPANEL_AGENT_CRYPTO_H
#define FPANEL_AGENT_CRYPTO_H

#include <stdio.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/base64.h>
#include <Logger/Logger.h>

class Crypto {
private:
    Logger logger;
public:

    std::string Encrypt(const std::string& str_in, const std::string& key, const std::string& iv)
    {
        try {
            std::string str_out;
            CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption encryption((byte*)key.c_str(), key.length(), (byte*)iv.c_str());
            CryptoPP::StringSource encryptor(str_in, true,
                                             new CryptoPP::StreamTransformationFilter(encryption,
                                                                                      new CryptoPP::Base64Encoder(
                                                                                              new CryptoPP::StringSink(str_out), false
                                                                                      )
                                             )
            );
            return str_out;
        }
        catch(...) {
            logger.Error("Encryption failed with error. String: %s", str_in.c_str());
            return "";
        }
    }

    std::string Decrypt(const std::string& str_in, const std::string& key, const std::string& iv)
    {
        try {
            std::string str_out;
            CryptoPP::CFB_Mode<CryptoPP::AES>::Decryption decryption((byte*)key.c_str(), key.length(), (byte*)iv.c_str());

            CryptoPP::StringSource decryptor(str_in, true,
                                             new CryptoPP::Base64Decoder(
                                                     new CryptoPP::StreamTransformationFilter(decryption,
                                                                                              new CryptoPP::StringSink(str_out)
                                                     )
                                             )
            );
            return str_out;
        }
        catch(...) {
            logger.Error("Decryption failed with error. String: %s", str_in.c_str());
            return "";
        }
    }
};
#endif //FPANEL_AGENT_CRYPTO_H