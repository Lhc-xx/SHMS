#ifndef SMART_HOME_MY_LOGGER_HPP
#define SMART_HOME_MY_LOGGER_HPP

#include <memory>
#include <mutex>
#include <string>

namespace shms {

class MyLogger {
public:
    // 包装类只暴露服务端需要的日志级别，并在 MyLogger.cc 中映射到
    // log4cpp 的优先级。
    enum class Level {
        Debug = 0,
        Info,
        Warn,
        Error
    };

    // 返回进程级日志对象。
    static MyLogger& instance();

    MyLogger(const MyLogger&) = delete;
    MyLogger& operator=(const MyLogger&) = delete;

    ~MyLogger();

    // 使用一个文件输出器配置 log4cpp。后续调用 initialize 会替换之前的
    // 输出器，便于受控启动期间重新加载配置。
    bool initialize(const std::string& logFile,
                    Level minimumLevel = Level::Info);

    // 分离并关闭文件输出器。重复调用 shutdown 也是安全的。
    void shutdown();

    // 查询日志状态，并按指定严重级别写入消息。
    bool initialized() const;
    bool write(Level level, const std::string& message);
    bool debug(const std::string& message);
    bool info(const std::string& message);
    bool warn(const std::string& message);
    bool error(const std::string& message);

    // 记录第一阶段服务端需求中规定的操作。接口有意不接收密码参数，密码
    // 绝不能写入日志。
    bool recordUserRegistration(const std::string& username, bool succeeded);
    bool recordUserLogin(const std::string& username, bool succeeded);
    bool recordCameraView(const std::string& username,
                          const std::string& cameraId);

    // 返回最近一次初始化或写入状态错误。
    std::string lastError() const;

private:
    // 将 log4cpp 头文件隐藏在公共接口之外，使只有日志实现及其构建目标
    // 依赖第三方库。
    struct Impl;

    MyLogger();

    static std::string sanitize(const std::string& value);

    // log4cpp 的 Category 具备线程安全性，此互斥锁还会将初始化、关闭与
    // 工作线程写日志操作串行化。
    mutable std::mutex mutex_;
    std::unique_ptr<Impl> impl_;
    std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_MY_LOGGER_HPP 头文件保护宏
