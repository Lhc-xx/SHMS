#include "PasswordHasher.hpp"
#include "UserService.hpp"

#include <cstdlib>
#include <iostream>
#include <map>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

class MemoryUserStore : public shms::UserStore {
public:
    MemoryUserStore() : failLookup_(false), failCreate_(false), nextId_(1) {}

    bool createUser(const std::string& name,
                    const std::string& setting,
                    const std::string& encrypt,
                    std::uint64_t* userId) override {
        if (failCreate_ || users_.find(name) != users_.end()) {
            lastError_ = "memory store create failed";
            return false;
        }
        shms::UserRecord record;
        record.id = nextId_++;
        record.name = name;
        record.setting = setting;
        record.encrypt = encrypt;
        users_[name] = record;
        if (userId != nullptr) {
            *userId = record.id;
        }
        lastError_.clear();
        return true;
    }

    bool findByName(const std::string& name,
                    shms::UserRecord* record,
                    bool* found) override {
        if (failLookup_ || record == nullptr || found == nullptr) {
            lastError_ = "memory store lookup failed";
            return false;
        }
        *record = shms::UserRecord();
        *found = false;
        std::map<std::string, shms::UserRecord>::const_iterator it =
            users_.find(name);
        if (it != users_.end()) {
            *record = it->second;
            *found = true;
        }
        lastError_.clear();
        return true;
    }

    std::string lastError() const override { return lastError_; }

    bool failLookup_;
    bool failCreate_;

private:
    std::uint64_t nextId_;
    std::map<std::string, shms::UserRecord> users_;
    std::string lastError_;
};

}  // 匿名命名空间

int main() {
    std::string hash;
    std::string error;
    expect(shms::PasswordHasher::hash("password",
                                      "$1$salt$",
                                      &hash,
                                      &error),
           "generate known MD5-crypt hash");
    expect(hash == "$1$salt$qJH7.N4xYta3aEG/dfqo/0",
           "match the standard MD5-crypt vector");

    bool matched = false;
    expect(shms::PasswordHasher::verify("password",
                                        "$1$salt$",
                                        hash,
                                        &matched,
                                        &error) &&
               matched,
           "verify the correct password");
    expect(shms::PasswordHasher::verify("wrong",
                                        "$1$salt$",
                                        hash,
                                        &matched,
                                        &error) &&
               !matched,
           "reject the wrong password");

    std::string setting;
    std::string generated;
    expect(shms::PasswordHasher::create("secret",
                                        &setting,
                                        &generated,
                                        &error),
           "create a salted password");
    expect(setting.size() >= 6U && setting.compare(0, 3, "$1$") == 0 &&
               setting[setting.size() - 1U] == '$' && generated.size() == 34U,
           "validate generated hash shape");

    MemoryUserStore store;
    shms::UserService service(store);
    shms::UserServiceResult result = service.registerUser("alice", "secret");
    expect(result.succeeded() && result.userId == 1U,
           "register a new user");
    result = service.registerUser("alice", "another");
    expect(result.code == shms::UserResultCode::UserAlreadyExists,
           "reject duplicate user");
    result = service.login("alice", "secret");
    expect(result.succeeded() && result.userId == 1U,
           "login with the correct password");
    result = service.login("alice", "wrong");
    expect(result.code == shms::UserResultCode::InvalidPassword,
           "reject an invalid password");
    result = service.login("missing", "secret");
    expect(result.code == shms::UserResultCode::UserNotFound,
           "report a missing user");
    result = service.registerUser("", "secret");
    expect(result.code == shms::UserResultCode::InvalidArgument,
           "reject an empty user name");

    store.failLookup_ = true;
    result = service.login("alice", "secret");
    expect(result.code == shms::UserResultCode::StorageError,
           "report lookup failures");

    std::cout << "All UserService tests passed" << std::endl;
    return EXIT_SUCCESS;
}
