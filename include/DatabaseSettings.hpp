#ifndef SMART_HOME_DATABASE_SETTINGS_HPP
#define SMART_HOME_DATABASE_SETTINGS_HPP

#include <cstdint>
#include <string>

namespace shms {

// 从部署环境读取数据库连接参数。数据库密码不进入配置文件、错误信息或日志。
// 未设置任何 SHMS_DB_* 变量时，数据库业务保持关闭，基础协议服务仍可启动。
class DatabaseSettings {
public:
    DatabaseSettings();

    // 读取 SHMS_DB_HOST、SHMS_DB_PORT、SHMS_DB_USER、SHMS_DB_PASSWORD 和
    // SHMS_DB_NAME。配置不完整或端口非法时返回 false，并保留旧配置。
    bool loadFromEnvironment(std::string* error = nullptr);

    bool enabled() const;
    std::string host() const;
    std::uint16_t port() const;
    std::string user() const;
    std::string password() const;
    std::string database() const;

private:
    bool enabled_;
    std::string host_;
    std::uint16_t port_;
    std::string user_;
    std::string password_;
    std::string database_;
};

}  // shms 命名空间

#endif  // SMART_HOME_DATABASE_SETTINGS_HPP 头文件保护宏
