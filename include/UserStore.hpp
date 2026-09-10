#ifndef SMART_HOME_USER_STORE_HPP
#define SMART_HOME_USER_STORE_HPP

#include <cstdint>
#include <string>

namespace shms {

struct UserRecord {
    std::uint64_t id;
    std::string name;
    std::string setting;
    std::string encrypt;

    UserRecord() : id(0) {}
};

// 用户业务服务依赖的用户存储抽象。UserDao 提供生产实现，单元测试可以
// 使用内存实现验证注册和登录流程，而无需连接真实数据库。
class UserStore {
public:
    virtual ~UserStore() {}

    // 创建用户并返回数据库生成的用户编号。
    virtual bool createUser(const std::string& name,
                            const std::string& setting,
                            const std::string& encrypt,
                            std::uint64_t* userId) = 0;

    // 按用户名查询用户。查询不到用户不是存储层错误，found 会被置为 false。
    virtual bool findByName(const std::string& name,
                            UserRecord* record,
                            bool* found) = 0;

    // 返回最近一次存储操作的错误信息。
    virtual std::string lastError() const = 0;
};

}  // shms 命名空间

#endif  // SMART_HOME_USER_STORE_HPP 头文件保护宏
