#include "UserService.hpp"

#include "MyLogger.hpp"
#include "PasswordHasher.hpp"

namespace shms {

UserService::UserService(UserStore& store, MyLogger* logger)
    : store_(store), logger_(logger) {}

UserServiceResult UserService::registerUser(const std::string& username,
                                            const std::string& password) {
    if (!validUsername(username) || !validPassword(password)) {
        if (validUsername(username)) {
            recordRegistration(username, false);
        }
        return UserServiceResult(UserResultCode::InvalidArgument,
                                 0,
                                 "invalid username or password");
    }

    UserRecord existing;
    bool found = false;
    if (!store_.findByName(username, &existing, &found)) {
        recordRegistration(username, false);
        return UserServiceResult(UserResultCode::StorageError,
                                 0,
                                 "user lookup failed");
    }
    if (found) {
        recordRegistration(username, false);
        return UserServiceResult(UserResultCode::UserAlreadyExists,
                                 existing.id,
                                 "user already exists");
    }

    std::string setting;
    std::string encrypt;
    std::string hashError;
    if (!PasswordHasher::create(password,
                                &setting,
                                &encrypt,
                                &hashError)) {
        recordRegistration(username, false);
        return UserServiceResult(UserResultCode::PasswordError,
                                 0,
                                 "password hashing failed");
    }

    std::uint64_t userId = 0;
    if (!store_.createUser(username, setting, encrypt, &userId)) {
        recordRegistration(username, false);
        return UserServiceResult(UserResultCode::StorageError,
                                 0,
                                 "user creation failed");
    }
    recordRegistration(username, true);
    return UserServiceResult(UserResultCode::Success,
                             userId,
                             "user registration succeeded");
}

UserServiceResult UserService::login(const std::string& username,
                                      const std::string& password) {
    if (!validUsername(username) || !validPassword(password)) {
        if (validUsername(username)) {
            recordLogin(username, false);
        }
        return UserServiceResult(UserResultCode::InvalidArgument,
                                 0,
                                 "invalid username or password");
    }

    UserRecord record;
    bool found = false;
    if (!store_.findByName(username, &record, &found)) {
        recordLogin(username, false);
        return UserServiceResult(UserResultCode::StorageError,
                                 0,
                                 "user lookup failed");
    }
    if (!found) {
        recordLogin(username, false);
        return UserServiceResult(UserResultCode::UserNotFound,
                                 0,
                                 "user not found");
    }

    bool matched = false;
    if (!PasswordHasher::verify(password,
                                record.setting,
                                record.encrypt,
                                &matched,
                                nullptr)) {
        recordLogin(username, false);
        return UserServiceResult(UserResultCode::PasswordError,
                                 0,
                                 "stored password format is invalid");
    }
    if (!matched) {
        recordLogin(username, false);
        return UserServiceResult(UserResultCode::InvalidPassword,
                                 0,
                                 "invalid password");
    }

    recordLogin(username, true);
    return UserServiceResult(UserResultCode::Success,
                             record.id,
                             "user login succeeded");
}

bool UserService::validUsername(const std::string& username) {
    if (username.empty() || username.size() > 20U ||
        username.find('\0') != std::string::npos) {
        return false;
    }
    for (std::string::const_iterator it = username.begin();
         it != username.end();
         ++it) {
        if (*it == '\r' || *it == '\n' || *it == '\t' || *it == ' ') {
            return false;
        }
    }
    return true;
}

bool UserService::validPassword(const std::string& password) {
    return !password.empty() && password.size() <= PasswordHasher::kMaxPasswordLength &&
           password.find('\0') == std::string::npos;
}

void UserService::recordRegistration(const std::string& username,
                                      bool succeeded) {
    if (logger_ != nullptr) {
        logger_->recordUserRegistration(username, succeeded);
    }
}

void UserService::recordLogin(const std::string& username, bool succeeded) {
    if (logger_ != nullptr) {
        logger_->recordUserLogin(username, succeeded);
    }
}

}  // shms 命名空间
