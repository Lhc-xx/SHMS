#include "Configuration.hpp"

#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>

namespace {

// Remove whitespace around a token while keeping whitespace inside it intact.
std::string trim(const std::string& value) {
    std::string::size_type begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    std::string::size_type end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

// Parse a decimal unsigned value without accepting signs, trailing text, or
// values outside the requested range.
bool parseUnsigned(const std::string& text,
                   unsigned long long maxValue,
                   unsigned long long* result) {
    const std::string value = trim(text);
    if (value.empty() || value[0] == '-') {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed =
        std::strtoull(value.c_str(), &end, 10);
    if (errno == ERANGE || end == value.c_str() || *end != '\0' ||
        parsed > maxValue) {
        return false;
    }

    *result = parsed;
    return true;
}

// Add a line number to parser errors so a bad server.conf can be fixed from
// the startup message without guessing which entry failed.
bool setError(std::string* error,
              std::size_t lineNumber,
              const std::string& message) {
    std::ostringstream stream;
    stream << "line " << lineNumber << ": " << message;
    *error = stream.str();
    return false;
}

}  // namespace

namespace shms {

Configuration::Values::Values()
    : port(0),
      threadNum(0),
      taskNum(0),
      loaded(false) {}

Configuration::Configuration() {}

Configuration& Configuration::instance() {
    // Function-local static initialization is the Meyers singleton pattern
    // and is synchronized by the C++11 runtime.
    static Configuration configuration;
    return configuration;
}

bool Configuration::load(const std::string& path) {
    // Open before taking the mutex. Parsing does not touch shared state and
    // therefore does not block readers during disk I/O.
    std::ifstream input(path.c_str());
    if (!input.is_open()) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = "cannot open configuration file: " + path;
        return false;
    }

    // Parse into a temporary candidate. This gives load() a strong exception-
    // and validation-failure guarantee: values_ changes only on success.
    Values candidate;
    bool hasIp = false;
    bool hasPort = false;
    bool hasThreadNum = false;
    bool hasTaskNum = false;
    bool hasVideoPath = false;
    bool hasLogFile = false;
    std::string error;
    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;

        // The configuration format uses '#' for both full-line and inline
        // comments, as shown in conf/server.conf.
        const std::string::size_type comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        // A configuration entry is intentionally strict: exactly one key and
        // one value prevents typos from silently changing server behavior.
        std::istringstream stream(line);
        std::string key;
        std::string value;
        std::string extra;
        if (!(stream >> key >> value) || (stream >> extra)) {
            setError(&error, lineNumber,
                     "expected exactly two fields: <key> <value>");
            break;
        }

        // Keep the accepted keys explicit. Unknown keys and duplicate keys are
        // rejected instead of being silently ignored.
        if (key == "ip") {
            if (hasIp) {
                setError(&error, lineNumber, "duplicate key: ip");
                break;
            }
            if (value.empty()) {
                setError(&error, lineNumber, "ip cannot be empty");
                break;
            }
            candidate.ip = value;
            hasIp = true;
        } else if (key == "port") {
            unsigned long long parsed = 0;
            if (hasPort) {
                setError(&error, lineNumber, "duplicate key: port");
                break;
            }
            if (!parseUnsigned(value, 65535, &parsed) || parsed == 0) {
                setError(&error, lineNumber,
                         "port must be an integer between 1 and 65535");
                break;
            }
            candidate.port = static_cast<std::uint16_t>(parsed);
            hasPort = true;
        } else if (key == "thread_num") {
            unsigned long long parsed = 0;
            if (hasThreadNum) {
                setError(&error, lineNumber, "duplicate key: thread_num");
                break;
            }
            if (!parseUnsigned(
                    value,
                    static_cast<unsigned long long>(
                        std::numeric_limits<std::size_t>::max()),
                    &parsed) ||
                parsed == 0) {
                setError(&error, lineNumber,
                         "thread_num must be a positive integer");
                break;
            }
            candidate.threadNum = static_cast<std::size_t>(parsed);
            hasThreadNum = true;
        } else if (key == "task_num") {
            unsigned long long parsed = 0;
            if (hasTaskNum) {
                setError(&error, lineNumber, "duplicate key: task_num");
                break;
            }
            if (!parseUnsigned(
                    value,
                    static_cast<unsigned long long>(
                        std::numeric_limits<std::size_t>::max()),
                    &parsed) ||
                parsed == 0) {
                setError(&error, lineNumber,
                         "task_num must be a positive integer");
                break;
            }
            candidate.taskNum = static_cast<std::size_t>(parsed);
            hasTaskNum = true;
        } else if (key == "video_path") {
            if (hasVideoPath) {
                setError(&error, lineNumber, "duplicate key: video_path");
                break;
            }
            if (value.empty()) {
                setError(&error, lineNumber, "video_path cannot be empty");
                break;
            }
            candidate.videoPath = value;
            hasVideoPath = true;
        } else if (key == "log_file") {
            if (hasLogFile) {
                setError(&error, lineNumber, "duplicate key: log_file");
                break;
            }
            if (value.empty()) {
                setError(&error, lineNumber, "log_file cannot be empty");
                break;
            }
            candidate.logFile = value;
            hasLogFile = true;
        } else {
            setError(&error, lineNumber, "unknown key: " + key);
            break;
        }
    }

    // Check required fields after parsing so a syntactically valid but
    // incomplete file cannot start the server with zero-valued settings.
    if (error.empty() && !hasIp) {
        error = "missing required key: ip";
    } else if (error.empty() && !hasPort) {
        error = "missing required key: port";
    } else if (error.empty() && !hasThreadNum) {
        error = "missing required key: thread_num";
    } else if (error.empty() && !hasTaskNum) {
        error = "missing required key: task_num";
    } else if (error.empty() && !hasVideoPath) {
        error = "missing required key: video_path";
    } else if (error.empty() && !hasLogFile) {
        error = "missing required key: log_file";
    }

    // Commit the complete candidate atomically from the point of view of
    // readers. On failure only lastError_ changes.
    std::lock_guard<std::mutex> lock(mutex_);
    if (!error.empty()) {
        lastError_ = error;
        return false;
    }

    candidate.loaded = true;
    values_ = candidate;
    lastError_.clear();
    return true;
}

bool Configuration::loaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.loaded;
}

std::string Configuration::ip() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.ip;
}

std::uint16_t Configuration::port() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.port;
}

std::size_t Configuration::threadNum() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.threadNum;
}

std::size_t Configuration::taskNum() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.taskNum;
}

std::string Configuration::videoPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.videoPath;
}

std::string Configuration::logFile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return values_.logFile;
}

std::string Configuration::lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

}  // namespace shms
