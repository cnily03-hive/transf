#include <iostream>

#include "ansi.h"
#include "utils.h"

template <typename T>
inline constexpr int to_int(T value) {
    return static_cast<int>(value);
}

class Logger {
    /**
     * Configuration declarations
     */
   public:
    enum struct Level { ERROR, WARN, INFO, DEBUG };
    enum struct Config {
        DISPLAY_IDENTIFIER,
        HIDE_LOG_PREFIX,
    };

   protected:
    struct LevelData {
        std::string prefix;
        std::string color;
    };

    LevelData level_data[4] = {
        {"[ERROR]", ansi::red},      // ERROR
        {"[WARN]", ansi::yellow},    // WARN
        {"[INFO]", ansi::blue},      // INFO
        {"[DEBUG]", ansi::magenta},  // DEBUG
    };

    bool configurations[2] = {
        false,  // DISPLAY_IDENTIFIER
        false,  // HIDE_LOG_PREFIX
    };

    /**
     * User functions
     */
   public:
    void set_identifier(std::string id) { identifier = id; }
    void set_level(Level lv) { level = lv; }

    void set_configuration(Config config, bool value) { configurations[to_int(config)] = value; }

    template <typename T>
    void set_color(Level lv, T color) {
        level_data[to_int(lv)].color = join_string(color);
    }

    std::string get_identifier() { return identifier; }
    std::string get_colored_prefix(Level lv) {
        std::ostringstream oss;
        oss << level_data[to_int(lv)].color << level_data[to_int(lv)].prefix << ansi::reset
            << " ";
        return oss.str();
    }

    /**
     * Print functions
     */
   public:
    template <typename... Args>
    void info(Args... args) {
        return _level_print(Level::INFO, args...);
    }

    template <typename... Args>
    void warn(Args... args) {
        return _level_print(Level::WARN, args...);
    }

    template <typename... Args>
    void error(Args... args) {
        return _level_print(Level::ERROR, args...);
    }

    template <typename... Args>
    void debug(Args... args) {
        return _level_print(Level::DEBUG, args...);
    }

    template <typename... Args>
    void log(Args... args) {
        std::string prefix =
            configurations[to_int(Config::HIDE_LOG_PREFIX)] ? "" : "\033[0;90m[LOG]\033[0m ";
        std::cout << join_string(prefix, args...) << std::endl;
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

    /**
     * Protected functions
     */
   protected:
    Level level;
    std::string identifier;

    template <typename... Args>
    void _level_print(Level lv, Args... args) {
        if (level < lv) return;
        std::cout << join_string(get_colored_prefix(lv), args...) << std::endl;
    }

    /**
     * Constructors
     */
   public:
    Logger(std::string id, Level lv = Level::INFO) : level(lv), identifier(id) {}
};
