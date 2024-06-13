#include <iostream>
#include <sstream>
#include <string>

#ifndef TITLE_INFO
#define TITLE_INFO "\033[0;34m[INFO]\033[0m "
#endif

#ifndef TITLE_ERROR
#define TITLE_ERROR "\033[0;31m[ERROR]\033[0m "
#endif

#ifndef TITLE_LOG
#define TITLE_LOG "\033[0;90m[LOG]\033[0m "
#endif

template <typename T, typename... Args>
std::string join_string(T first, Args... args) {
    std::ostringstream oss;
    oss << first;
    ((oss << args), ...);
    return oss.str();
}

namespace logger {

template <typename... Args>
void info(Args... args) {
    std::cout << join_string(TITLE_INFO, args...) << std::endl;
}
template <typename... Args>
void error(Args... args) {
    std::cerr << join_string(TITLE_ERROR, args...) << std::endl;
}
template <typename... Args>
void log(Args... args) {
    std::cout << join_string(TITLE_LOG, args...) << std::endl;
}
template <typename... Args>
void print(Args... args) {
    std::cout << join_string(args...) << std::endl;
}
template <typename... Args>
void instant(Args... args) {
    std::cout << join_string(args...);
    std::cout.flush();
}
}  // namespace logger