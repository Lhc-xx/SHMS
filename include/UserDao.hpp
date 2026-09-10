#ifndef SMART_HOME_USER_DAO_HPP
#define SMART_HOME_USER_DAO_HPP

#include "MySqlClient.hpp"

#include <cstdint>
#include <string>

namespace shms {

struct UserRecord {
    std::uint64_t id;
    std::string name;
    std::string setting;
    std::string encrypt;
};

// 数据库设计文档中 t_user 表对应的 DAO。密码哈希有意放在此类之外；此类
// 只接收生成后的 setting 和密文，并在读写时都使用预处理语句。
class UserDao {
public:
    explicit UserDao(MySqlClient& client);
    UserDao(const UserDao&) = delete;
    UserDao& operator=(const UserDao&) = delete;

    // 如果数据表不存在则创建。生产部署可以单独执行 database/schema.sql，
    // 测试则可以安全地重复调用此方法。
    bool initializeSchema();

    // 插入一个用户。setting/encrypt 已由账号服务生成；绝不能将明文密码传入
    // 此方法。
    bool createUser(const std::string& name,
                    const std::string& setting,
                    const std::string& encrypt,
                    std::uint64_t* userId = nullptr);

    // 查询用户时，不将“未找到”视为数据库故障。
    bool findByName(const std::string& name,
                    UserRecord* record,
                    bool* found);

    std::string lastError() const;

private:
    bool setError(const std::string& message);
    void clearError();

    MySqlClient& client_;
    std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_USER_DAO_HPP 头文件保护宏
