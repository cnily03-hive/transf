#include <sstream>
#include <string>

template <typename T, typename... Args>
std::string join_string(T first, Args... args) {
    std::ostringstream oss;
    oss << first;
    ((oss << args), ...);
    return oss.str();
}
