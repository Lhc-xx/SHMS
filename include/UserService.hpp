#ifndef SMART_HOME_USER_SERVICE_HPP
#define SMART_HOME_USER_SERVICE_HPP

#include "UserStore.hpp"

#include <cstdint>
#include <string>

namespace shms {

class MyLogger;

enum class UserResultCode {
    Success,
    InvalidArgument,
    UserAlreadyExists,
    UserNotFound,
    InvalidPassword,
    StorageError,
    PasswordError
};

struct UserServiceResult {
    UserResultCode code;
    std::uint64_t userId;
    std::string message;

    UserServiceResult(UserResultCode resultCode,
                      std::uint64_t resultUserId,
                      const std::string& resultMessage)
        : code(resultCode), userId(resultUserId), message(resultMessage) {}

    bool succeeded() const { return code == UserResultCode::Success; }
};

// 用户业务服务，负责注册、登录、密码哈希校验和业务操作日志记录。
// 数据库访问通过 UserStore 注入，避免业务层直接拼接 SQL 或依赖具体数据库实现。
class UserService {
public:
    explicit UserService(UserStore& store, MyLogger* logger = nullptr);
    UserService(const UserService&) = delete;
    UserService& operator=(const UserService&) = delete;

    // 注册用户。明文密码只在本次调用期间存在，绝不写入存储层或日志。
    UserServiceResult registerUser(const std::string& username,
                                   const std::string& password);

    // 登录用户，按“查询用户—使用 setting 计算哈希—常量时间比较”的流程校验密码。
    UserServiceResult login(const std::string& username,
                            const std::string& password);

private:
    static bool validUsername(const std::string& username);
    static bool validPassword(const std::string& password);

    void recordRegistration(const std::string& username, bool succeeded);
    void recordLogin(const std::string& username, bool succeeded);

    UserStore& store_;
    MyLogger* logger_;
};

}  // shms 命名空间

#endif  // SMART_HOME_USER_SERVICE_HPP 头文件保护宏
