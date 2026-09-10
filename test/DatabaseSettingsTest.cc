#include "DatabaseSettings.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void setEnvironment(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value == nullptr ? "" : value);
#else
    if (value == nullptr) {
        unsetenv(name);
    } else {
        setenv(name, value, 1);
    }
#endif
}

void clearDatabaseEnvironment() {
    setEnvironment("SHMS_DB_HOST", nullptr);
    setEnvironment("SHMS_DB_PORT", nullptr);
    setEnvironment("SHMS_DB_USER", nullptr);
    setEnvironment("SHMS_DB_PASSWORD", nullptr);
    setEnvironment("SHMS_DB_NAME", nullptr);
}

}  // 匿名命名空间

int main() {
    clearDatabaseEnvironment();

    shms::DatabaseSettings settings;
    std::string error;
    expect(settings.loadFromEnvironment(&error),
           "load without database environment");
    expect(!settings.enabled(), "keep database disabled without environment");
    expect(settings.port() == 3306, "use the default database port");

    setEnvironment("SHMS_DB_HOST", "127.0.0.1");
    setEnvironment("SHMS_DB_PASSWORD", "test-secret");
    expect(!settings.loadFromEnvironment(&error),
           "reject incomplete database environment");
    expect(error.find("SHMS_DB_USER") != std::string::npos,
           "identify the missing database user variable");
    expect(error.find("test-secret") == std::string::npos,
           "never include the database password in an error");
    expect(!settings.enabled(), "keep the previous disabled state on failure");

    setEnvironment("SHMS_DB_USER", "shms");
    setEnvironment("SHMS_DB_NAME", "smart_home_monitor");
    setEnvironment("SHMS_DB_PORT", "3307");
    expect(settings.loadFromEnvironment(&error),
           "load a complete database environment");
    expect(settings.enabled(), "enable the database with complete settings");
    expect(settings.host() == "127.0.0.1", "read the database host");
    expect(settings.port() == 3307, "read the database port");
    expect(settings.user() == "shms", "read the database user");
    expect(settings.password() == "test-secret", "read the database password");
    expect(settings.database() == "smart_home_monitor",
           "read the database name");

    setEnvironment("SHMS_DB_PORT", "70000");
    expect(!settings.loadFromEnvironment(&error),
           "reject an out-of-range database port");
    expect(settings.port() == 3307,
           "keep the previous database settings on port failure");

    clearDatabaseEnvironment();
    std::cout << "All DatabaseSettings tests passed" << std::endl;
    return EXIT_SUCCESS;
}
