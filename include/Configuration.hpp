#ifndef SMART_HOME_CONFIGURATION_HPP
#define SMART_HOME_CONFIGURATION_HPP

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace shms {

class Configuration {
public:
    // 返回进程级配置对象。函数内静态变量提供单例的延迟初始化，并由 C++11
    // 运行时保证启动过程的线程安全。
    static Configuration& instance();

    Configuration(const Configuration&) = delete;
    Configuration& operator=(const Configuration&) = delete;

    // 加载并校验配置文件。加载失败时保持当前配置不变，调用方可以安全地
    // 重试错误的配置，而不会丢失上一次的有效值。
    bool load(const std::string& path);

    // 只读访问器。每个访问器都会在持有互斥锁时复制字符串，使调用方与
    // 内部存储相互独立。
    bool loaded() const;
    std::string ip() const;
    std::uint16_t port() const;
    std::size_t threadNum() const;
    std::size_t taskNum() const;
    std::string videoPath() const;
    std::string logFile() const;
    // 返回最近一次加载或日志相关的错误信息。
    std::string lastError() const;

private:
    Configuration();

    // 配置值先在临时对象中解析，只有所有必需字段校验通过后才一次性提交。
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

    // 配置通常在启动时加载，但互斥锁也保证并发读取和后续重新加载行为明确。
    mutable std::mutex mutex_;
    Values values_;
    std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_CONFIGURATION_HPP 头文件保护宏
