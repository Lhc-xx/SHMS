#ifndef SMART_HOME_PASSWORD_HASHER_HPP
#define SMART_HOME_PASSWORD_HASHER_HPP

#include <cstddef>
#include <string>

namespace shms {

// 提供与 Unix MD5-crypt（$1$）格式兼容的密码哈希能力。
// 该算法仅用于兼容项目接口文档，新增系统不应将 MD5-crypt 作为首选密码算法。
class PasswordHasher {
public:
    static const std::size_t kSaltLength = 8;
    static const std::size_t kMaxPasswordLength = 128;

    // 使用随机盐值生成 setting 和完整的 encrypt 哈希字符串。
    static bool create(const std::string& password,
                       std::string* setting,
                       std::string* encrypt,
                       std::string* error = nullptr);

    // 使用指定的 $1$ 前缀和盐值生成完整的 MD5-crypt 哈希字符串。
    static bool hash(const std::string& password,
                     const std::string& setting,
                     std::string* encrypt,
                     std::string* error = nullptr);

    // 使用同样的 setting 校验明文密码，matched 只表示密码是否匹配。
    static bool verify(const std::string& password,
                       const std::string& setting,
                       const std::string& encrypt,
                       bool* matched,
                       std::string* error = nullptr);
};

}  // shms 命名空间

#endif  // SMART_HOME_PASSWORD_HASHER_HPP 头文件保护宏
