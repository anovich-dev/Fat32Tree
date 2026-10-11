#include "fat32tree/core/Logger.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace fat32tree
{

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

std::filesystem::path Logger::initialize()
{
    Logger& logger = instance();
    logger.initializeImpl();

    return logger.m_logFilePath;
}

void Logger::info(const std::string& message)
{
    instance().write("INFO", message);
}

void Logger::warning(const std::string& message)
{
    instance().write("WARNING", message);
}

void Logger::error(const std::string& message)
{
    instance().write("ERROR", message);
}

void Logger::debug(const std::string& message)
{
    instance().write("DEBUG", message);
}

void Logger::initializeImpl()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_logFile.is_open())
    {
        m_logFile.close();
    }

    m_logFilePath = generateLogFilePath();

    m_logFile.open(m_logFilePath, std::ios::out | std::ios::app);

    if (!m_logFile.is_open())
    {
        throw std::runtime_error("Failed to open log file: " + m_logFilePath.string());
    }
}

void Logger::write(const std::string& level, const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_logFile.is_open())
    {
        std::cerr
            << "[LOGGER ERROR] Logger is not initialized. Message: "
            << message
            << '\n';

        return;
    }

    m_logFile
        << '[' << currentTimestamp() << "] "
        << '[' << level << "] "
        << message
        << '\n';

    m_logFile.flush();
}

std::filesystem::path Logger::getDesktopPath() const
{
#ifdef _WIN32

    const char* userProfile = std::getenv("USERPROFILE");

    if (userProfile == nullptr)
    {
        throw std::runtime_error("Failed to determine Windows user profile directory");
    }

    return std::filesystem::path{userProfile} / "Desktop";

#else

    const char* home = std::getenv("HOME");

    if (home == nullptr)
    {
        throw std::runtime_error("Failed to determine user home directory");
    }

    return std::filesystem::path{home} / "Desktop";

#endif
}

std::filesystem::path Logger::generateLogFilePath() const
{
    const std::filesystem::path desktopPath = getDesktopPath();

    if (!std::filesystem::exists(desktopPath) ||
        !std::filesystem::is_directory(desktopPath))
    {
        throw std::runtime_error("Desktop directory does not exist: " + desktopPath.string());
    }

    const std::string baseName = "Logger_" + fileTimestamp();

    std::filesystem::path logFilePath = desktopPath / (baseName + ".log");

    std::size_t suffix = 1;

    while (std::filesystem::exists(logFilePath))
    {
        logFilePath =
            desktopPath /
            (baseName +
             "_" +
             std::to_string(suffix) +
             ".log");

        ++suffix;
    }

    return logFilePath;
}

std::string Logger::currentTimestamp() const
{
    const auto now = std::chrono::system_clock::now();

    const auto time = std::chrono::system_clock::to_time_t(now);

    const std::tm localTime = getLocalTime(time);

    std::ostringstream stream;

    stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");

    return stream.str();
}

std::string Logger::fileTimestamp() const
{
    const auto now = std::chrono::system_clock::now();

    const auto time = std::chrono::system_clock::to_time_t(now);

    const std::tm localTime = getLocalTime(time);

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream stream;

    stream
        << std::put_time(&localTime, "%Y%m%d_%H%M%S")
        << '_'
        << std::setfill('0')
        << std::setw(3)
           << milliseconds.count();

    return stream.str();
}

std::tm Logger::getLocalTime(std::time_t time) const
{
    std::tm result{};

#ifdef _WIN32

    if (localtime_s(&result, &time) != 0)
    {
        throw std::runtime_error("Failed to convert timestamp to local time");
    }

#else

    if (localtime_r(&time, &result) == nullptr)
    {
        throw std::runtime_error("Failed to convert timestamp to local time");
    }

#endif

    return result;
}

} // namespace fat32tree
