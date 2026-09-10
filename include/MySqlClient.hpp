#ifndef SMART_HOME_MYSQL_CLIENT_HPP
#define SMART_HOME_MYSQL_CLIENT_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace shms {

// Small RAII wrapper around the MySQL C client. One client serializes its
// operations; callers that need parallel queries should create a connection
// per worker rather than sharing a MYSQL handle without synchronization.
class MySqlClient {
public:
    MySqlClient();
    MySqlClient(const MySqlClient&) = delete;
    MySqlClient& operator=(const MySqlClient&) = delete;

    ~MySqlClient();

    // Connect using credentials supplied by the deployment environment. The
    // password is never copied into an error message or log record.
    bool connect(const std::string& host,
                 std::uint16_t port,
                 const std::string& user,
                 const std::string& password,
                 const std::string& database);
    void disconnect();
    bool connected() const;

    // Execute parameterized INSERT/UPDATE/DELETE/DDL. Each '?' in sql must
    // have one corresponding value in parameters.
    bool execute(const std::string& sql,
                 const std::vector<std::string>& parameters,
                 std::uint64_t* affectedRows = nullptr,
                 std::uint64_t* insertId = nullptr);

    // Execute a parameterized SELECT and return rows as strings. NULL fields
    // are represented by an empty string because the current DAO schema does
    // not use nullable user fields.
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

}  // namespace shms

#endif  // SMART_HOME_MYSQL_CLIENT_HPP
