#ifndef SMART_HOME_CONFIGURATION_HPP
#define SMART_HOME_CONFIGURATION_HPP

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace shms {

class Configuration {
public:
    static Configuration& instance();

    Configuration(const Configuration&) = delete;
    Configuration& operator=(const Configuration&) = delete;

    // Load and validate a configuration file. The current configuration is
    // left unchanged when loading fails.
    bool load(const std::string& path);

    bool loaded() const;
    std::string ip() const;
    std::uint16_t port() const;
    std::size_t threadNum() const;
    std::size_t taskNum() const;
    std::string videoPath() const;
    std::string logFile() const;
    std::string lastError() const;

private:
    Configuration();

    struct Values {
        std::string ip;
        std::uint16_t port;
        std::size_t threadNum;
        std::size_t taskNum;
        std::string videoPath;
        std::string logFile;
        bool loaded;

        Values();
    };

    mutable std::mutex mutex_;
    Values values_;
    std::string lastError_;
};

}  // namespace shms

#endif  // SMART_HOME_CONFIGURATION_HPP
