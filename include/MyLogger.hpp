#ifndef SMART_HOME_MY_LOGGER_HPP
#define SMART_HOME_MY_LOGGER_HPP

#include <memory>
#include <mutex>
#include <string>

namespace shms {

class MyLogger {
public:
    // The wrapper exposes only the levels needed by the server. They map to
    // log4cpp priorities in MyLogger.cc.
    enum class Level {
        Debug = 0,
        Info,
        Warn,
        Error
    };

    // Return the process-wide logger instance.
    static MyLogger& instance();

    MyLogger(const MyLogger&) = delete;
    MyLogger& operator=(const MyLogger&) = delete;

    ~MyLogger();

    // Configure log4cpp with one file appender. A later initialize call
    // replaces the previous appender, which is useful after a configuration
    // reload during controlled startup.
    bool initialize(const std::string& logFile,
                    Level minimumLevel = Level::Info);

    // Detach and close the file appender. Calling shutdown more than once is
    // safe.
    void shutdown();

    // Query logger state and write messages at the requested severity.
    bool initialized() const;
    bool write(Level level, const std::string& message);
    bool debug(const std::string& message);
    bool info(const std::string& message);
    bool warn(const std::string& message);
    bool error(const std::string& message);

    // Record the operations required by the first-phase server requirements.
    // Passwords are intentionally not accepted by these interfaces and must
    // never be written to the log.
    bool recordUserRegistration(const std::string& username, bool succeeded);
    bool recordUserLogin(const std::string& username, bool succeeded);
    bool recordCameraView(const std::string& username,
                          const std::string& cameraId);

    // Return the most recent initialization or write-state error.
    std::string lastError() const;

private:
    // Keep log4cpp headers out of the public interface so only the logger
    // implementation and its build target depend on the third-party library.
    struct Impl;

    MyLogger();

    static std::string sanitize(const std::string& value);

    // log4cpp categories are thread-safe, and this mutex additionally
    // serializes initialization/shutdown with writes from worker threads.
    mutable std::mutex mutex_;
    std::unique_ptr<Impl> impl_;
    std::string lastError_;
};

}  // namespace shms

#endif  // SMART_HOME_MY_LOGGER_HPP
