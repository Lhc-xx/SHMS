#include "Configuration.hpp"

#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>

namespace {

// 删除令牌两侧的空白，同时保留令牌内部的空白。
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

// 解析十进制无符号数，不接受符号、尾随文本或超出指定范围的值。
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

// 为解析错误添加行号，使错误的 server.conf 可以根据启动消息直接修复，
// 无需猜测是哪一项配置失败。
bool setError(std::string* error,
              std::size_t lineNumber,
              const std::string& message) {
    std::ostringstream stream;
    stream << "line " << lineNumber << ": " << message;
    *error = stream.str();
    return false;
}

}  // 匿名命名空间

namespace shms {

Configuration::Values::Values()
    : port(0),
      threadNum(0),
      taskNum(0),
      loaded(false) {}

Configuration::Configuration() {}

Configuration& Configuration::instance() {
    // 函数内静态变量初始化采用 Meyers 单例模式，并由 C++11 运行时同步。
    static Configuration configuration;
    return configuration;
}

bool Configuration::load(const std::string& path) {
    // 在获取互斥锁之前打开文件。解析过程不访问共享状态，因此磁盘 I/O 不会
    // 阻塞读取配置的线程。
    std::ifstream input(path.c_str());
    if (!input.is_open()) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = "cannot open configuration file: " + path;
        return false;
    }

    // 解析到临时配置对象中。这样可以保证 load() 在异常或校验失败时保持
    // 强保证：values_ 只有在成功后才会改变。
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

        // 配置格式使用 '#' 表示整行注释和行内注释，具体格式见
        // conf/server.conf。
        const std::string::size_type comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        // 配置项格式有意保持严格：恰好一个键和一个值，避免拼写错误静默地
        // 改变服务端行为。
        std::istringstream stream(line);
        std::string key;
        std::string value;
        std::string extra;
        if (!(stream >> key >> value) || (stream >> extra)) {
            setError(&error, lineNumber,
                     "expected exactly two fields: <key> <value>");
            break;
        }

        // 明确列出可接受的键。未知键和重复键会被拒绝，不会被静默忽略。
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

    // 解析后检查必需字段，避免语法有效但内容不完整的配置文件以零值设置
    // 启动服务端。
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

    // 从读取方视角原子提交完整的临时配置。失败时只有 lastError_ 会改变。
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

}  // shms 命名空间
