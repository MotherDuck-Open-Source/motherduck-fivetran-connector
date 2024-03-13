#pragma once

#include <iostream>
#include <map>
#include "logging_sink.grpc.pb.h"

namespace mdlog {

    class MdLog {
    public:
        explicit MdLog(std::unique_ptr<logging_sink::LoggingSink::Stub>& _client);
        void log(const std::string level, const std::string message);

        void info(std::string message);

        void warning(std::string message);

        void severe(std::string message);
    private:
        std::unique_ptr<logging_sink::LoggingSink::Stub> client;
    };

std::string escape_char(const std::string &str, const char &c);


} // namespace mdlog
