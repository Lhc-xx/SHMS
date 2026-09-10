#include "DatabaseSettings.hpp"

#include <cerrno>
#include <cstdlib>
#include <limits>

namespace {

struct EnvironmentValue {
    bool present;
    std::string value;

    EnvironmentValue() : present(false) {}
};

EnvironmentValue readEnvironment(const char* name) {
    EnvironmentValue result;
    const char* value = std::getenv(name);
    if (value != nullptr) {
        result.present = true;
        result.value = value;
    }
    return result;
}

bool parsePort(const std::string& text, std::uint16_t* port) {
    if (text.empty() || port == nullptr) {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed =
        std::strtoull(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' ||
        parsed == 0 ||
        parsed > static_cast<unsigned long long>(
                      std::numeric_limits<std::uint16_t>::max())) {
        return false;
    }
    *port = static_cast<std::uint16_t>(parsed);
    return true;
}

bool setError(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

}  // 匿名命名空间

namespace shms {

DatabaseSettings::DatabaseSettings()
    : enabled_(false), port_(3306) {}

bool DatabaseSettings::loadFromEnvironment(std::string* error) {
    EnvironmentValue host = readEnvironment("SHMS_DB_HOST");
    EnvironmentValue port = readEnvironment("SHMS_DB_PORT");
    EnvironmentValue user = readEnvironment("SHMS_DB_USER");
    EnvironmentValue password = readEnvironment("SHMS_DB_PASSWORD");
    EnvironmentValue database = readEnvironment("SHMS_DB_NAME");

    const bool anyConfigured = host.present || port.present || user.present ||
                               password.present || database.present;
    if (!anyConfigured) {
        enabled_ = false;
        host_.clear();
        port_ = 3306;
        user_.clear();
        password_.clear();
        database_.clear();
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }

    if (!host.present || host.value.empty()) {
        return setError(error, "SHMS_DB_HOST is required when database is enabled");
    }
    if (!user.present || user.value.empty()) {
        return setError(error, "SHMS_DB_USER is required when database is enabled");
    }
    if (!password.present) {
        return setError(error,
                        "SHMS_DB_PASSWORD is required when database is enabled");
    }
    if (!database.present || database.value.empty()) {
        return setError(error, "SHMS_DB_NAME is required when database is enabled");
    }

    std::uint16_t parsedPort = 3306;
    if (port.present && !parsePort(port.value, &parsedPort)) {
        return setError(error, "SHMS_DB_PORT must be an integer between 1 and 65535");
    }

    enabled_ = true;
    host_ = host.value;
    port_ = parsedPort;
    user_ = user.value;
    password_ = password.value;
    database_ = database.value;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool DatabaseSettings::enabled() const {
    return enabled_;
}

std::string DatabaseSettings::host() const {
    return host_;
}

std::uint16_t DatabaseSettings::port() const {
    return port_;
}

std::string DatabaseSettings::user() const {
    return user_;
}

std::string DatabaseSettings::password() const {
    return password_;
}

std::string DatabaseSettings::database() const {
    return database_;
}

}  // shms 命名空间
