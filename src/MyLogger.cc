#include "MyLogger.hpp"

#include <log4cpp/Category.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/Priority.hh>

#include <memory>
#include <stdexcept>

namespace {

// Translate the project-level enum to the corresponding log4cpp priority.
log4cpp::Priority::Value toLog4cppPriority(shms::MyLogger::Level level) {
    switch (level) {
        case shms::MyLogger::Level::Debug:
            return log4cpp::Priority::DEBUG;
        case shms::MyLogger::Level::Info:
            return log4cpp::Priority::INFO;
        case shms::MyLogger::Level::Warn:
            return log4cpp::Priority::WARN;
        case shms::MyLogger::Level::Error:
            return log4cpp::Priority::ERROR;
    }
    return log4cpp::Priority::INFO;
}

}  // namespace

namespace shms {

struct MyLogger::Impl {
    Impl()
        : category(nullptr),
          minimumLevel(Level::Info),
          initialized(false) {}

    // Category is owned by log4cpp's hierarchy. The pointer is non-owning;
    // the appender attached to it is owned by the Category.
    log4cpp::Category* category;
    Level minimumLevel;
    bool initialized;
    std::string logFile;
};

MyLogger::MyLogger()
    : impl_(new Impl()) {}

MyLogger::~MyLogger() {
    shutdown();
}

MyLogger& MyLogger::instance() {
    static MyLogger logger;
    return logger;
}

bool MyLogger::initialize(const std::string& logFile, Level minimumLevel) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile.empty()) {
        lastError_ = "log file path cannot be empty";
        return false;
    }

    // Remove an old appender before installing the new one. This avoids
    // duplicate records when initialize() is called more than once.
    if (impl_->category != nullptr) {
        impl_->category->removeAllAppenders();
        impl_->category = nullptr;
        impl_->initialized = false;
        impl_->logFile.clear();
    }

    log4cpp::Category* category = nullptr;
    try {
        // Use a dedicated category with additivity disabled so messages are
        // written only to the configured server file, not to an inherited
        // root appender such as stdout.
        category = &log4cpp::Category::getInstance(
            "SmartHomeMonitoringSystem");
        category->removeAllAppenders();
        category->setAdditivity(false);
        category->setPriority(toLog4cppPriority(minimumLevel));

        // The layout contains timestamp, priority, category, and message;
        // %l adds milliseconds for diagnosing concurrent server events.
        std::unique_ptr<log4cpp::PatternLayout> layout(
            new log4cpp::PatternLayout());
        layout->setConversionPattern(
            "%d{%Y-%m-%d %H:%M:%S,%l} [%p] [%c] %m%n");

        // FileAppender opens in append mode so restarting the service keeps
        // the existing audit trail. addAppender(pointer) transfers ownership
        // to the Category after the layout is attached.
        std::unique_ptr<log4cpp::FileAppender> appender(
            new log4cpp::FileAppender(
                "SmartHomeMonitoringSystemFileAppender", logFile, true));
        appender->setLayout(layout.release());
        category->addAppender(appender.release());

        impl_->category = category;
        impl_->minimumLevel = minimumLevel;
        impl_->initialized = true;
        impl_->logFile = logFile;
        lastError_.clear();
        return true;
    } catch (const std::exception& exception) {
        // A failed open or invalid layout must not leave a half-configured
        // logger attached to the global Category hierarchy.
        if (category != nullptr) {
            category->removeAllAppenders();
        }
        impl_->category = nullptr;
        impl_->initialized = false;
        impl_->logFile.clear();
        lastError_ = std::string("log4cpp initialization failed: ") +
                     exception.what();
        return false;
    }
}

void MyLogger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Category::removeAllAppenders() closes and detaches the FileAppender;
    // subsequent writes will fail fast until initialize() is called again.
    if (impl_->category != nullptr) {
        impl_->category->removeAllAppenders();
    }
    impl_->category = nullptr;
    impl_->initialized = false;
    impl_->logFile.clear();
}

bool MyLogger::initialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_->initialized;
}

bool MyLogger::write(Level level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!impl_->initialized || impl_->category == nullptr) {
        lastError_ = "logger is not initialized";
        return false;
    }
    // Treat filtered messages as a successful no-op. Callers do not need to
    // branch around debug logging when production runs at INFO level.
    if (static_cast<int>(level) < static_cast<int>(impl_->minimumLevel)) {
        return true;
    }

    // Use the std::string overload so user-provided '%' characters are logged
    // as data rather than interpreted as printf format directives.
    impl_->category->log(toLog4cppPriority(level), message);
    return true;
}

bool MyLogger::debug(const std::string& message) {
    return write(Level::Debug, message);
}

bool MyLogger::info(const std::string& message) {
    return write(Level::Info, message);
}

bool MyLogger::warn(const std::string& message) {
    return write(Level::Warn, message);
}

bool MyLogger::error(const std::string& message) {
    return write(Level::Error, message);
}

bool MyLogger::recordUserRegistration(const std::string& username,
                                      bool succeeded) {
    // Record the outcome but never accept a password argument.
    return write(
        succeeded ? Level::Info : Level::Warn,
        std::string("user registration ") + (succeeded ? "succeeded" :
                                               "failed") +
            ": username=" + sanitize(username));
}

bool MyLogger::recordUserLogin(const std::string& username, bool succeeded) {
    return write(succeeded ? Level::Info : Level::Warn,
                 std::string("user login ") +
                     (succeeded ? "succeeded" : "failed") +
                     ": username=" + sanitize(username));
}

bool MyLogger::recordCameraView(const std::string& username,
                                const std::string& cameraId) {
    return write(Level::Info,
                 "camera viewed: username=" + sanitize(username) +
                     ", camera_id=" + sanitize(cameraId));
}

std::string MyLogger::lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

std::string MyLogger::sanitize(const std::string& value) {
    // Keep one business operation on one physical log line. This also avoids
    // allowing a user-controlled newline to forge a second log record.
    std::string result = value;
    for (std::string::iterator it = result.begin(); it != result.end(); ++it) {
        if (*it == '\r' || *it == '\n') {
            *it = ' ';
        }
    }
    return result;
}

}  // namespace shms
