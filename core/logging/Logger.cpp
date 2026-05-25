#include "Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace local_jarvis::logging {
namespace {

std::string utcTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc {};

#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif

    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

} // namespace

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::info(const std::string &message)
{
    write("INFO", message);
}

void Logger::warn(const std::string &message)
{
    write("WARN", message);
}

void Logger::error(const std::string &message)
{
    write("ERROR", message);
}

void Logger::write(const char *level, const std::string &message)
{
    std::lock_guard lock(m_mutex);
    std::clog << "[" << utcTimestamp() << "] [" << level << "] " << message << '\n';
}

} // namespace local_jarvis::logging
