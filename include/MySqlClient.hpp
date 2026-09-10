#ifndef SMART_HOME_MYSQL_CLIENT_HPP
#define SMART_HOME_MYSQL_CLIENT_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace shms {

// MySQL C 客户端的轻量 RAII 包装器。单个客户端会串行执行自身操作；需要
// 并发查询时，调用方应为每个工作线程创建连接，不要在无同步的情况下共享
// MYSQL 句柄。
class MySqlClient {
public:
    MySqlClient();
    MySqlClient(const MySqlClient&) = delete;
    MySqlClient& operator=(const MySqlClient&) = delete;

    ~MySqlClient();

    // 使用部署环境提供的凭据连接。密码绝不会被复制到错误信息或日志记录中。
    bool connect(const std::string& host,
                 std::uint16_t port,
                 const std::string& user,
                 const std::string& password,
                 const std::string& database);
    void disconnect();
    bool connected() const;

    // 执行参数化 INSERT/UPDATE/DELETE/DDL。sql 中的每个 '?' 都必须在
    // parameters 中有一个对应值。
    bool execute(const std::string& sql,
                 const std::vector<std::string>& parameters,
                 std::uint64_t* affectedRows = nullptr,
                 std::uint64_t* insertId = nullptr);

    // 执行参数化 SELECT，并以字符串形式返回数据行。由于当前 DAO 表结构
    // 不使用可为空的用户字段，NULL 字段会表示为空字符串。
    bool query(const std::string& sql,
               const std::vector<std::string>& parameters,
               std::vector<std::vector<std::string> >* rows);

    std::string lastError() const;

private:
    struct Impl;

    bool setError(const std::string& message);
    void clearError();

    std::unique_ptr<Impl> impl_;
    mutable std::mutex mutex_;
    mutable std::mutex errorMutex_;
    std::string lastError_;
};

}  // shms 命名空间

#endif  // SMART_HOME_MYSQL_CLIENT_HPP 头文件保护宏
