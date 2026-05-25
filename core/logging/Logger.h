#pragma once

#include <mutex>
#include <string>

namespace local_jarvis::logging {

class Logger {
public:
    static Logger &instance();

    void info(const std::string &message);
    void warn(const std::string &message);
    void error(const std::string &message);

private:
    Logger() = default;

    void write(const char *level, const std::string &message);

    std::mutex m_mutex;
};

} // namespace local_jarvis::logging
