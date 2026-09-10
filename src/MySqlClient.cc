#include "MySqlClient.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <type_traits>
#include <utility>

#include <mysql/mysql.h>

namespace {

std::string mysqlError(MYSQL* connection, const char* operation) {
    std::ostringstream stream;
    stream << operation << ": "
           << (connection == nullptr ? "unknown MySQL error"
                                     : mysql_error(connection));
    return stream.str();
}

std::string statementError(MYSQL_STMT* statement, const char* operation) {
    std::ostringstream stream;
    stream << operation << ": "
           << (statement == nullptr ? "unknown MySQL statement error"
                                     : mysql_stmt_error(statement));
    return stream.str();
}

bool bindParameters(MYSQL_STMT* statement,
                    const std::vector<std::string>& parameters,
                    std::vector<MYSQL_BIND>* binds,
                    std::vector<unsigned long>* lengths) {
    if (parameters.empty()) {
        return true;
    }

    binds->resize(parameters.size());
    lengths->resize(parameters.size());
    std::memset(binds->data(), 0, sizeof(MYSQL_BIND) * binds->size());
    for (std::size_t index = 0; index < parameters.size(); ++index) {
        (*lengths)[index] = static_cast<unsigned long>(parameters[index].size());
        MYSQL_BIND& bind = (*binds)[index];
        bind.buffer_type = MYSQL_TYPE_STRING;
        bind.buffer = parameters[index].empty()
                          ? nullptr
                          : const_cast<char*>(parameters[index].data());
        bind.buffer_length = (*lengths)[index];
        bind.length = &(*lengths)[index];
    }
    return mysql_stmt_bind_param(statement, binds->data()) == 0;
}

}  // 匿名命名空间

namespace shms {

struct MySqlClient::Impl {
    Impl() : connection(nullptr) {}

    MYSQL* connection;
};

MySqlClient::MySqlClient() : impl_(new Impl()) {}

MySqlClient::~MySqlClient() {
    disconnect();
}

bool MySqlClient::connect(const std::string& host,
                          std::uint16_t port,
                          const std::string& user,
                          const std::string& password,
                          const std::string& database) {
    disconnect();

    std::lock_guard<std::mutex> lock(mutex_);

    MYSQL* connection = mysql_init(nullptr);
    if (connection == nullptr) {
        return setError("mysql_init failed");
    }

    const char charset[] = "utf8mb4";
    if (mysql_options(connection, MYSQL_SET_CHARSET_NAME, charset) != 0) {
        const std::string error = mysqlError(connection, "mysql_options failed");
        mysql_close(connection);
        return setError(error);
    }

    if (mysql_real_connect(connection,
                           host.c_str(),
                           user.c_str(),
                           password.c_str(),
                           database.c_str(),
                           static_cast<unsigned int>(port),
                           nullptr,
                           0) == nullptr) {
        const std::string error = mysqlError(connection, "mysql connect failed");
        mysql_close(connection);
        return setError(error);
    }

    impl_->connection = connection;
    clearError();
    return true;
}

void MySqlClient::disconnect() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (impl_ != nullptr && impl_->connection != nullptr) {
            mysql_close(impl_->connection);
            impl_->connection = nullptr;
        }
    }
    clearError();
}

bool MySqlClient::connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return impl_ != nullptr && impl_->connection != nullptr;
}

bool MySqlClient::execute(const std::string& sql,
                          const std::vector<std::string>& parameters,
                          std::uint64_t* affectedRows,
                          std::uint64_t* insertId) {
    if (!connected()) {
        return setError("MySQL client is not connected");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (impl_ == nullptr || impl_->connection == nullptr) {
        return setError("MySQL client is not connected");
    }
    if (affectedRows != nullptr) {
        *affectedRows = 0;
    }
    if (insertId != nullptr) {
        *insertId = 0;
    }

    MYSQL_STMT* statement = mysql_stmt_init(impl_->connection);
    if (statement == nullptr) {
        return setError(mysqlError(impl_->connection, "mysql_stmt_init failed"));
    }
    if (mysql_stmt_prepare(statement,
                           sql.c_str(),
                           static_cast<unsigned long>(sql.size())) != 0) {
        const std::string error = statementError(statement, "mysql prepare failed");
        mysql_stmt_close(statement);
        return setError(error);
    }

    std::vector<MYSQL_BIND> binds;
    std::vector<unsigned long> lengths;
    if (!bindParameters(statement, parameters, &binds, &lengths)) {
        const std::string error = statementError(statement, "mysql bind failed");
        mysql_stmt_close(statement);
        return setError(error);
    }
    if (mysql_stmt_execute(statement) != 0) {
        const std::string error = statementError(statement, "mysql execute failed");
        mysql_stmt_close(statement);
        return setError(error);
    }

    if (affectedRows != nullptr) {
        *affectedRows = static_cast<std::uint64_t>(
            mysql_stmt_affected_rows(statement));
    }
    if (insertId != nullptr) {
        *insertId = static_cast<std::uint64_t>(mysql_stmt_insert_id(statement));
    }
    mysql_stmt_close(statement);
    clearError();
    return true;
}

bool MySqlClient::query(const std::string& sql,
                        const std::vector<std::string>& parameters,
                        std::vector<std::vector<std::string> >* rows) {
    if (rows == nullptr) {
        return setError("query output cannot be null");
    }
    rows->clear();
    if (!connected()) {
        return setError("MySQL client is not connected");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (impl_ == nullptr || impl_->connection == nullptr) {
        return setError("MySQL client is not connected");
    }

    MYSQL_STMT* statement = mysql_stmt_init(impl_->connection);
    if (statement == nullptr) {
        return setError(mysqlError(impl_->connection, "mysql_stmt_init failed"));
    }
    if (mysql_stmt_prepare(statement,
                           sql.c_str(),
                           static_cast<unsigned long>(sql.size())) != 0) {
        const std::string error = statementError(statement, "mysql prepare failed");
        mysql_stmt_close(statement);
        return setError(error);
    }

    std::vector<MYSQL_BIND> binds;
    std::vector<unsigned long> lengths;
    if (!bindParameters(statement, parameters, &binds, &lengths)) {
        const std::string error = statementError(statement, "mysql bind failed");
        mysql_stmt_close(statement);
        return setError(error);
    }
    if (mysql_stmt_execute(statement) != 0) {
        const std::string error = statementError(statement, "mysql execute failed");
        mysql_stmt_close(statement);
        return setError(error);
    }
    if (mysql_stmt_store_result(statement) != 0) {
        const std::string error = statementError(statement, "mysql store result failed");
        mysql_stmt_close(statement);
        return setError(error);
    }

    MYSQL_RES* metadata = mysql_stmt_result_metadata(statement);
    if (metadata == nullptr) {
        const std::string error = statementError(statement, "mysql result metadata failed");
        mysql_stmt_free_result(statement);
        mysql_stmt_close(statement);
        return setError(error);
    }

    const unsigned int fieldCount = mysql_num_fields(metadata);
    MYSQL_FIELD* fields = mysql_fetch_fields(metadata);
    std::vector<MYSQL_BIND> resultBinds(fieldCount);
    std::vector<std::vector<char> > buffers(fieldCount);
    std::vector<unsigned long> resultLengths(fieldCount, 0);
    typedef std::remove_pointer<
        decltype(std::declval<MYSQL_BIND>().is_null)>::type BindBool;
    std::unique_ptr<BindBool[]> isNull(new BindBool[fieldCount]());
    std::unique_ptr<BindBool[]> errors(new BindBool[fieldCount]());
    if (!resultBinds.empty()) {
        std::memset(resultBinds.data(),
                    0,
                    sizeof(MYSQL_BIND) * resultBinds.size());
    }

    for (unsigned int index = 0; index < fieldCount; ++index) {
        const unsigned long bufferSize = fields[index].max_length == 0
                                              ? 1
                                              : fields[index].max_length + 1;
        buffers[index].resize(bufferSize);
        resultBinds[index].buffer_type = MYSQL_TYPE_STRING;
        resultBinds[index].buffer = buffers[index].data();
        resultBinds[index].buffer_length = bufferSize;
        resultBinds[index].length = &resultLengths[index];
        resultBinds[index].is_null = &isNull[index];
        resultBinds[index].error = &errors[index];
    }

    if (fieldCount > 0 && mysql_stmt_bind_result(statement, resultBinds.data()) != 0) {
        const std::string error = statementError(statement, "mysql bind result failed");
        mysql_free_result(metadata);
        mysql_stmt_free_result(statement);
        mysql_stmt_close(statement);
        return setError(error);
    }

    int fetchStatus = 0;
    while ((fetchStatus = mysql_stmt_fetch(statement)) == 0 ||
           fetchStatus == MYSQL_DATA_TRUNCATED) {
        std::vector<std::string> row;
        row.reserve(fieldCount);
        for (unsigned int index = 0; index < fieldCount; ++index) {
            if (isNull[index]) {
                row.push_back(std::string());
            } else {
                const std::size_t length =
                    std::min<std::size_t>(resultLengths[index],
                                          buffers[index].size());
                row.push_back(std::string(buffers[index].data(), length));
            }
        }
        rows->push_back(std::move(row));
    }
    if (fetchStatus != MYSQL_NO_DATA) {
        const std::string error = statementError(statement, "mysql fetch failed");
        mysql_free_result(metadata);
        mysql_stmt_free_result(statement);
        mysql_stmt_close(statement);
        return setError(error);
    }

    mysql_free_result(metadata);
    mysql_stmt_free_result(statement);
    mysql_stmt_close(statement);
    clearError();
    return true;
}

std::string MySqlClient::lastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

bool MySqlClient::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = message;
    return false;
}

void MySqlClient::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

}  // shms 命名空间
