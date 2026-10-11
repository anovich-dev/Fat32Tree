#pragma once

#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace fat32tree
{

class Logger
{
public:
    static std::filesystem::path initialize();

    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);
    static void debug(const std::string& message);

private:
    Logger() = default;

    static Logger& instance();

    void initializeImpl();
    void write(const std::string& level, const std::string& message);

    std::filesystem::path getDesktopPath() const;
    std::filesystem::path generateLogFilePath() const;

    std::string currentTimestamp() const;
    std::string fileTimestamp() const;

    std::tm getLocalTime(std::time_t time) const;

private:
    std::ofstream m_logFile;
    std::filesystem::path m_logFilePath;
    std::mutex m_mutex;
};

} // namespace fat32tree
