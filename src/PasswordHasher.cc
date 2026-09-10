#include "PasswordHasher.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <random>

namespace {

const char kMd5CryptMagic[] = "$1$";
const char kCryptBase64[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
const char kSaltAlphabet[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

class Md5Context {
public:
    Md5Context()
        : state_{0x67452301U, 0xefcdab89U, 0x98badcfeU, 0x10325476U},
          bitCount_(0),
          bufferSize_(0) {}

    void update(const void* data, std::size_t size) {
        if (data == nullptr || size == 0) {
            return;
        }

        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        bitCount_ += static_cast<std::uint64_t>(size) * 8U;
        while (size != 0) {
            const std::size_t available = 64U - bufferSize_;
            const std::size_t copied = std::min(size, available);
            std::memcpy(buffer_ + bufferSize_, bytes, copied);
            bufferSize_ += copied;
            bytes += copied;
            size -= copied;
            if (bufferSize_ == 64U) {
                transform(buffer_);
                bufferSize_ = 0;
            }
        }
    }

    void final(unsigned char digest[16]) {
        const std::uint64_t originalBitCount = bitCount_;
        const unsigned char one = 0x80U;
        update(&one, 1);
        const unsigned char zero = 0;
        while (bufferSize_ != 56U) {
            update(&zero, 1);
        }

        unsigned char length[8];
        for (std::size_t i = 0; i < 8U; ++i) {
            length[i] = static_cast<unsigned char>(
                (originalBitCount >> (8U * i)) & 0xffU);
        }
        update(length, sizeof(length));

        for (std::size_t i = 0; i < 4U; ++i) {
            digest[i * 4U] = static_cast<unsigned char>(state_[i] & 0xffU);
            digest[i * 4U + 1U] =
                static_cast<unsigned char>((state_[i] >> 8U) & 0xffU);
            digest[i * 4U + 2U] =
                static_cast<unsigned char>((state_[i] >> 16U) & 0xffU);
            digest[i * 4U + 3U] =
                static_cast<unsigned char>((state_[i] >> 24U) & 0xffU);
        }
    }

private:
    static std::uint32_t rotateLeft(std::uint32_t value, unsigned int shift) {
        return (value << shift) | (value >> (32U - shift));
    }

    void transform(const unsigned char block[64]) {
        static const std::uint32_t k[64] = {
            0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU,
            0xf57c0fafU, 0x4787c62aU, 0xa8304613U, 0xfd469501U,
            0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU,
            0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U,
            0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU,
            0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
            0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU,
            0xa9e3e905U, 0xfcefa3f8U, 0x676f02d9U, 0x8d2a4c8aU,
            0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU,
            0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
            0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U,
            0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,
            0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U,
            0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU, 0x85845dd1U,
            0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U,
            0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U};
        static const unsigned int shifts[64] = {
            7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
            5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

        std::uint32_t words[16];
        for (std::size_t i = 0; i < 16U; ++i) {
            words[i] = static_cast<std::uint32_t>(block[i * 4U]) |
                       (static_cast<std::uint32_t>(block[i * 4U + 1U]) << 8U) |
                       (static_cast<std::uint32_t>(block[i * 4U + 2U]) << 16U) |
                       (static_cast<std::uint32_t>(block[i * 4U + 3U]) << 24U);
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        for (std::size_t i = 0; i < 64U; ++i) {
            std::uint32_t function = 0;
            std::size_t wordIndex = 0;
            if (i < 16U) {
                function = (b & c) | ((~b) & d);
                wordIndex = i;
            } else if (i < 32U) {
                function = (d & b) | ((~d) & c);
                wordIndex = (5U * i + 1U) % 16U;
            } else if (i < 48U) {
                function = b ^ c ^ d;
                wordIndex = (3U * i + 5U) % 16U;
            } else {
                function = c ^ (b | (~d));
                wordIndex = (7U * i) % 16U;
            }

            const std::uint32_t previousD = d;
            d = c;
            c = b;
            b += rotateLeft(a + function + k[i] + words[wordIndex], shifts[i]);
            a = previousD;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
    }

    std::uint32_t state_[4];
    std::uint64_t bitCount_;
    std::size_t bufferSize_;
    unsigned char buffer_[64];
};

bool parseSetting(const std::string& setting, std::string* salt) {
    if (salt == nullptr || setting.size() < 5U ||
        setting.compare(0, 3, kMd5CryptMagic) != 0 ||
        setting[setting.size() - 1U] != '$') {
        return false;
    }

    const std::size_t saltStart = 3U;
    const std::size_t saltLength = setting.size() - saltStart - 1U;
    if (saltLength == 0U || saltLength > 8U) {
        return false;
    }
    for (std::size_t i = 0; i < saltLength; ++i) {
        if (std::strchr(kSaltAlphabet, setting[saltStart + i]) == nullptr) {
            return false;
        }
    }
    *salt = setting.substr(saltStart, saltLength);
    return true;
}

void appendCryptBase64(std::string* output,
                       unsigned int value,
                       unsigned int count) {
    for (unsigned int i = 0; i < count; ++i) {
        output->push_back(kCryptBase64[value & 0x3fU]);
        value >>= 6U;
    }
}

bool md5Crypt(const std::string& password,
              const std::string& setting,
              std::string* encrypt) {
    std::string salt;
    if (!parseSetting(setting, &salt) || encrypt == nullptr) {
        return false;
    }

    Md5Context context;
    context.update(password.data(), password.size());
    context.update(kMd5CryptMagic, 3U);
    context.update(salt.data(), salt.size());

    Md5Context alternateContext;
    alternateContext.update(password.data(), password.size());
    alternateContext.update(salt.data(), salt.size());
    alternateContext.update(password.data(), password.size());
    std::array<unsigned char, 16> alternate;
    alternateContext.final(alternate.data());

    for (std::size_t remaining = password.size(); remaining != 0U;) {
        const std::size_t copied = std::min(remaining, alternate.size());
        context.update(alternate.data(), copied);
        remaining -= copied;
    }

    for (std::size_t count = password.size(); count != 0U; count >>= 1U) {
        if ((count & 1U) != 0U) {
            const unsigned char zero = 0;
            context.update(&zero, 1U);
        } else {
            context.update(password.data(), 1U);
        }
    }

    std::array<unsigned char, 16> digest;
    context.final(digest.data());
    for (unsigned int round = 0; round < 1000U; ++round) {
        Md5Context roundContext;
        if ((round & 1U) != 0U) {
            roundContext.update(password.data(), password.size());
        } else {
            roundContext.update(digest.data(), digest.size());
        }
        if (round % 3U != 0U) {
            roundContext.update(salt.data(), salt.size());
        }
        if (round % 7U != 0U) {
            roundContext.update(password.data(), password.size());
        }
        if ((round & 1U) != 0U) {
            roundContext.update(digest.data(), digest.size());
        } else {
            roundContext.update(password.data(), password.size());
        }
        roundContext.final(digest.data());
    }

    std::string result = std::string(kMd5CryptMagic) + salt + "$";
    appendCryptBase64(&result,
                      (static_cast<unsigned int>(digest[0]) << 16U) |
                          (static_cast<unsigned int>(digest[6]) << 8U) |
                          digest[12],
                      4U);
    appendCryptBase64(&result,
                      (static_cast<unsigned int>(digest[1]) << 16U) |
                          (static_cast<unsigned int>(digest[7]) << 8U) |
                          digest[13],
                      4U);
    appendCryptBase64(&result,
                      (static_cast<unsigned int>(digest[2]) << 16U) |
                          (static_cast<unsigned int>(digest[8]) << 8U) |
                          digest[14],
                      4U);
    appendCryptBase64(&result,
                      (static_cast<unsigned int>(digest[3]) << 16U) |
                          (static_cast<unsigned int>(digest[9]) << 8U) |
                          digest[15],
                      4U);
    appendCryptBase64(&result,
                      (static_cast<unsigned int>(digest[4]) << 16U) |
                          (static_cast<unsigned int>(digest[10]) << 8U) |
                          digest[5],
                      4U);
    appendCryptBase64(&result, digest[11], 2U);
    *encrypt = result;
    return true;
}

bool setError(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool validPassword(const std::string& password, std::string* error) {
    if (password.empty() || password.size() > shms::PasswordHasher::kMaxPasswordLength) {
        return setError(error, "password length must contain 1 to 128 bytes");
    }
    if (password.find('\0') != std::string::npos) {
        return setError(error, "password cannot contain a null byte");
    }
    return true;
}

bool constantTimeEquals(const std::string& left, const std::string& right) {
    const std::size_t maxSize = std::max(left.size(), right.size());
    unsigned char difference =
        static_cast<unsigned char>(left.size() ^ right.size());
    for (std::size_t i = 0; i < maxSize; ++i) {
        const unsigned char leftByte =
            i < left.size() ? static_cast<unsigned char>(left[i]) : 0U;
        const unsigned char rightByte =
            i < right.size() ? static_cast<unsigned char>(right[i]) : 0U;
        difference = static_cast<unsigned char>(difference | (leftByte ^ rightByte));
    }
    return difference == 0U;
}

}  // 匿名命名空间

namespace shms {

bool PasswordHasher::create(const std::string& password,
                            std::string* setting,
                            std::string* encrypt,
                            std::string* error) {
    if (setting == nullptr || encrypt == nullptr) {
        return setError(error, "setting and encrypt outputs cannot be null");
    }
    setting->clear();
    encrypt->clear();
    if (!validPassword(password, error)) {
        return false;
    }

    try {
        std::random_device random;
        std::string salt;
        salt.reserve(kSaltLength);
        for (std::size_t i = 0; i < kSaltLength; ++i) {
            salt.push_back(kSaltAlphabet[random() % (sizeof(kSaltAlphabet) - 1U)]);
        }
        *setting = std::string(kMd5CryptMagic) + salt + "$";
    } catch (...) {
        return setError(error, "failed to generate password salt");
    }

    return hash(password, *setting, encrypt, error);
}

bool PasswordHasher::hash(const std::string& password,
                          const std::string& setting,
                          std::string* encrypt,
                          std::string* error) {
    if (encrypt == nullptr) {
        return setError(error, "encrypt output cannot be null");
    }
    if (!validPassword(password, error)) {
        return false;
    }
    if (!md5Crypt(password, setting, encrypt)) {
        return setError(error, "setting must use the $1$<salt>$ format");
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool PasswordHasher::verify(const std::string& password,
                            const std::string& setting,
                            const std::string& encrypt,
                            bool* matched,
                            std::string* error) {
    if (matched == nullptr) {
        return setError(error, "matched output cannot be null");
    }
    *matched = false;
    std::string calculated;
    if (!hash(password, setting, &calculated, error)) {
        return false;
    }
    *matched = constantTimeEquals(calculated, encrypt);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

}  // shms 命名空间
