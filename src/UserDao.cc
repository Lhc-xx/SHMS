#include "UserDao.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <vector>

namespace {

const char kCreateUserTableSql[] =
    "CREATE TABLE IF NOT EXISTS t_user ("
    "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
    "name VARCHAR(20) NOT NULL,"
    "setting CHAR(64) NOT NULL,"
    "encrypt CHAR(64) NOT NULL,"
    "PRIMARY KEY (id),"
    "UNIQUE KEY uk_t_user_name (name)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

bool parseId(const std::string& text, std::uint64_t* value) {
    if (text.empty() || value == nullptr) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed =
        std::strtoull(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' ||
        parsed > std::numeric_limits<std::uint64_t>::max()) {
        return false;
    }
    *value = static_cast<std::uint64_t>(parsed);
    return true;
}

}  // 匿名命名空间

namespace shms {

UserDao::UserDao(MySqlClient& client) : client_(client) {}

bool UserDao::initializeSchema() {
    if (!client_.execute(kCreateUserTableSql, std::vector<std::string>())) {
        return setError(client_.lastError());
    }
    clearError();
    return true;
}

bool UserDao::createUser(const std::string& name,
                         const std::string& setting,
                         const std::string& encrypt,
                         std::uint64_t* userId) {
    if (name.empty() || name.size() > 20) {
        return setError("user name must contain 1 to 20 characters");
    }
    if (setting.empty() || setting.size() > 64) {
        return setError("user setting must contain 1 to 64 characters");
    }
    if (encrypt.empty() || encrypt.size() > 64) {
        return setError("user encrypt value must contain 1 to 64 characters");
    }

    const char sql[] =
        "INSERT INTO t_user (name, setting, encrypt) VALUES (?, ?, ?)";
    std::vector<std::string> parameters;
    parameters.push_back(name);
    parameters.push_back(setting);
    parameters.push_back(encrypt);
    std::uint64_t insertId = 0;
    if (!client_.execute(sql, parameters, nullptr, &insertId)) {
        return setError(client_.lastError());
    }
    if (userId != nullptr) {
        *userId = insertId;
    }
    clearError();
    return true;
}

bool UserDao::findByName(const std::string& name,
                         UserRecord* record,
                         bool* found) {
    if (record == nullptr || found == nullptr) {
        return setError("findByName requires record and found outputs");
    }
    *record = UserRecord();
    *found = false;
    if (name.empty() || name.size() > 20) {
        return setError("user name must contain 1 to 20 characters");
    }

    const char sql[] =
        "SELECT id, name, setting, encrypt FROM t_user "
        "WHERE name = ? LIMIT 1";
    std::vector<std::string> parameters(1, name);
    std::vector<std::vector<std::string> > rows;
    if (!client_.query(sql, parameters, &rows)) {
        return setError(client_.lastError());
    }
    if (rows.empty()) {
        clearError();
        return true;
    }
    if (rows[0].size() != 4 || !parseId(rows[0][0], &record->id)) {
        return setError("unexpected t_user result shape");
    }
    record->name = rows[0][1];
    record->setting = rows[0][2];
    record->encrypt = rows[0][3];
    *found = true;
    clearError();
    return true;
}

std::string UserDao::lastError() const {
    return lastError_;
}

bool UserDao::setError(const std::string& message) {
    lastError_ = message;
    return false;
}

void UserDao::clearError() {
    lastError_.clear();
}

}  // shms 命名空间
