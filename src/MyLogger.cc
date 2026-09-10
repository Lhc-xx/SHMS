#include "MyLogger.hpp"

#include <log4cpp/Category.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/Priority.hh>

#include <memory>
#include <stdexcept>

namespace {

// 将项目级日志枚举转换为对应的 log4cpp 优先级。
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

}  // 匿名命名空间

namespace shms {

struct MyLogger::Impl {
    Impl()
        : category(nullptr),
          minimumLevel(Level::Info),
          initialized(false) {}

    // Category 由 log4cpp 层级结构负责拥有。此处指针不拥有对象；附加到
    // Category 的输出器由 Category 负责管理。
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

    // 安装新输出器前先移除旧输出器，避免多次调用 initialize() 时产生重复
    // 日志记录。
    if (impl_->category != nullptr) {
        impl_->category->removeAllAppenders();
        impl_->category = nullptr;
        impl_->initialized = false;
        impl_->logFile.clear();
    }

    log4cpp::Category* category = nullptr;
    try {
        // 使用关闭继承的专用 Category，使消息只写入配置的服务端日志文件，
        // 不写入 stdout 等继承自根 Category 的输出器。
        category = &log4cpp::Category::getInstance(
            "SmartHomeMonitoringSystem");
        category->removeAllAppenders();
        category->setAdditivity(false);
        category->setPriority(toLog4cppPriority(minimumLevel));

        // 日志布局包含时间戳、优先级、Category 和消息；%l 添加毫秒信息，
        // 便于诊断并发服务端事件。
        std::unique_ptr<log4cpp::PatternLayout> layout(
            new log4cpp::PatternLayout());
        layout->setConversionPattern(
            "%d{%Y-%m-%d %H:%M:%S,%l} [%p] [%c] %m%n");

        // FileAppender 以追加模式打开文件，服务重启后仍能保留已有审计记录。
        // 设置布局后，addAppender(pointer) 会将输出器所有权转移给 Category。
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
        // 打开文件失败或布局无效时，不能让半配置状态的日志对象残留在全局
        // Category 层级中。
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
    // Category::removeAllAppenders() 会关闭并分离 FileAppender；在再次调用
    // initialize() 前，后续写入会快速失败。
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
    // 将被过滤的消息视为成功的空操作。生产环境使用 INFO 级别时，调用方
    // 无需为 DEBUG 日志额外编写分支。
    if (static_cast<int>(level) < static_cast<int>(impl_->minimumLevel)) {
        return true;
    }

    // 使用 std::string 重载，使用户提供的 '%' 字符按普通数据记录，而不会
    // 被解释为 printf 格式指令。
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
    // 记录操作结果，但绝不接收密码参数。
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
    // 让一次业务操作对应一条物理日志行，同时避免用户控制的换行符伪造
    // 第二条日志记录。
    std::string result = value;
    for (std::string::iterator it = result.begin(); it != result.end(); ++it) {
        if (*it == '\r' || *it == '\n') {
            *it = ' ';
        }
    }
    return result;
}

}  // shms 命名空间
