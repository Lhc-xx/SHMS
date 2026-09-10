#include "MySqlClient.hpp"
#include "UserDao.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main() {
    // The unit test does not require deployment credentials or a running
    // database. It verifies validation and deterministic disconnected errors;
    // the cloud integration command in README covers a real MySQL instance.
    shms::MySqlClient client;
    expect(!client.connected(), "start disconnected");

    std::vector<std::vector<std::string> > rows;
    expect(!client.query("SELECT 1", std::vector<std::string>(), &rows),
           "reject query before connection");
    expect(client.lastError().find("not connected") != std::string::npos,
           "explain disconnected query");

    shms::UserDao dao(client);
    expect(!dao.createUser("", "salt", "cipher"),
           "reject empty user name before database access");
    expect(dao.lastError().find("1 to 20") != std::string::npos,
           "explain user name validation");
    expect(!dao.createUser("alice", "", "cipher"),
           "reject empty setting before database access");
    expect(!dao.createUser("alice", "salt", ""),
           "reject empty ciphertext before database access");

    shms::UserRecord record;
    bool found = false;
    expect(!dao.findByName("", &record, &found),
           "reject empty lookup name");
    expect(!found, "leave lookup result false on validation failure");

    std::cout << "All Database tests passed" << std::endl;
    return EXIT_SUCCESS;
}
