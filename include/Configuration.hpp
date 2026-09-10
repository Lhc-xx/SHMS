#ifndef SMART_HOME_CONFIGURATION_HPP
#define SMART_HOME_CONFIGURATION_HPP

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace shms {

class Configuration {
public:
    // Return the process-wide configuration object. The function-local static
    // gives the singleton lazy initialization and C++11 thread-safe startup.
    static Configuration& instance();

    Configuration(const Configuration&) = delete;
    Configuration& operator=(const Configuration&) = delete;

    // Load and validate a configuration file. The current configuration is
    // left unchanged when loading fails, so callers can safely retry a bad
    // reload without losing the last valid values.
    bool load(const std::string& path);

    // Read-only accessors. Each accessor copies strings while holding the
    // mutex, which keeps callers independent from the internal storage.
    bool loaded() const;
    std::string ip() const;
    std::uint16_t port() const;
    std::size_t threadNum() const;
    std::size_t taskNum() const;
    std::string videoPath() const;
    std::string logFile() const;
    // Return the most recent load or logger-facing error message.
    std::string lastError() const;

private:
    Configuration();

    // Values is parsed off to the side and committed as one object only after
    // every required field has passed validation.
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

    // Configuration is normally loaded during startup, but the mutex also
    // makes concurrent reads and a future reload well-defined.
    mutable std::mutex mutex_;
    Values values_;
    std::string lastError_;
};

}  // namespace shms

#endif  // SMART_HOME_CONFIGURATION_HPP
