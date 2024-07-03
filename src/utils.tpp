#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

typedef unsigned long u_long;

template <typename T, typename... Args>
std::string join_string(T first, Args... args) {
    std::ostringstream oss;
    oss << first;
    ((oss << args), ...);
    return oss.str();
}

std::string fmt_size(u_long size) {
    std::string unit[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    u_long s = size;
    int p = 0;
    while (s >= 1024 && i < 4) {
        p = s % 1024 * 100 / 1024 % 100;
        s /= 1024;
        ++i;
    }
    std::string p_str = p < 10 ? ("0" + std::to_string(p))
                               : (p % 10 == 0 ? std::to_string(p / 10) : std::to_string(p));

    std::string num_str = std::to_string(s) + (p == 0 ? "" : "." + p_str);
    return num_str + " " + unit[i];
}

std::string uuid_v1() {
    static std::mt19937_64 rng(std::random_device{}());
    static std::uniform_int_distribution<uint64_t> dist(0, (1ULL << 48) - 1);

    // get current time in 100 nanoseconds
    auto now = std::chrono::system_clock::now().time_since_epoch();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count() / 100;

    // split timestamp into high, mid, low parts
    uint16_t time_low = timestamp & 0xFFFF;
    uint16_t time_mid = (timestamp >> 16) & 0xFFFF;
    uint16_t time_hi_and_version = (timestamp >> 32) & 0xFFFF;
    time_hi_and_version = (time_hi_and_version & 0x0FFF) | (1 << 12);  // 设置版本号为 1

    // generate clock sequence
    uint16_t clock_seq = dist(rng) & 0x3FFF;
    clock_seq = (clock_seq & 0x3FFF) | 0x8000;  // 设置两个最高有效位为 1 和 0

    // generate node
    uint64_t node = dist(rng);

    // assemble UUID
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << time_low << '-' << std::setw(4)
       << time_mid << '-' << std::setw(4) << time_hi_and_version << '-' << std::setw(4) << clock_seq
       << '-' << std::setw(12) << node;

    return ss.str();
}