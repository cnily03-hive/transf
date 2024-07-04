#include <iostream>

#include "ansi.h"
#include "utils.h"

template <typename T>
inline constexpr int to_int(T value) {
    return static_cast<int>(value);
}

#define CRLF "\r\n"
#define LF "\n"

#ifdef _WIN32
#define END_LINE CRLF
#define IS_CRLF 1
#define IS_LF 0
#else
#define END_LINE LF
#define IS_CRLF 0
#define IS_LF 1
#endif

class Logger {
    /**
     * Configuration declarations
     */
   public:
    enum struct Level { DEBUG, INFO, WARN, ERROR };
    enum struct Config {
        DISPLAY_IDENTIFIER,
        HIDE_LOG_PREFIX,
        LINE_CRLF,
    };

    /**
     * Protected functions
     */
   protected:
    Level level;
    std::string identifier;

    template <typename... Args>
    void _level_out(std::ostream& os, Level lv, Args... args) const {
        if (lv < level) return;
        os << join_string(get_colored_prefix(lv), args...) << std::endl;
    }

    /**
     * Constructors
     */
   public:
    Logger(std::string id, Level lv = Level::INFO) : level(lv), identifier(id) {}

   protected:
    struct LevelData {
        std::string prefix;
        std::string color;
    };

    LevelData level_data[4] = {
        {"[DEBUG]", ansi::magenta},  // DEBUG
        {"[INFO]", ansi::blue},      // INFO
        {"[WARN]", ansi::yellow},    // WARN
        {"[ERROR]", ansi::red},      // ERROR
    };

    bool configurations[3] = {
        false,    // DISPLAY_IDENTIFIER
        false,    // HIDE_LOG_PREFIX
        IS_CRLF,  // LINE_CRLF
    };

    char end_line[3] = {IS_CRLF ? '\r' : '\n', IS_CRLF ? '\n' : '\0', '\0'};

    /**
     * User functions
     */
   public:
    // End a line and flush the buffer
    void endl() { std::cout << std::endl; }

    void set_identifier(std::string id) { identifier = id; }
    void set_level(Level lv) { level = lv; }
    Level get_level() const { return level; }

    void set_configuration(Config config, bool value) {
        configurations[to_int(config)] = value;
        if (config == Config::LINE_CRLF) {
            end_line[0] = value ? '\r' : '\n';
            end_line[1] = value ? '\n' : '\0';
        }
    }

    template <typename T>
    void set_color(Level lv, T color) {
        level_data[to_int(lv)].color = join_string(color);
    }

    std::string get_identifier() { return identifier; }
    std::string get_colored_prefix(Level lv) const {
        std::ostringstream oss;
        oss << level_data[to_int(lv)].color << level_data[to_int(lv)].prefix << ansi::reset << " ";
        return oss.str();
    }

    /**
     * Print functions
     */
   public:
    template <typename... Args>
    void level_print(Level lv, Args... args) const {
        if (lv < level) return;
        auto& os = lv == Level::ERROR || lv == Level::WARN ? std::cerr : std::cout;
        os << join_string(args...) << std::endl;
    }

    template <typename... Args>
    void info(Args... args) const {
        return _level_out(std::cout, Level::INFO, args...);
    }

    template <typename... Args>
    void warn(Args... args) const {
        return _level_out(std::cerr, Level::WARN, args...);
    }

    template <typename... Args>
    void error(Args... args) const {
        return _level_out(std::cerr, Level::ERROR, args...);
    }

    template <typename... Args>
    void debug(Args... args) const {
        return _level_out(std::cout, Level::DEBUG, args...);
    }

    template <typename... Args>
    void log(Args... args) const {
        std::string prefix =
            configurations[to_int(Config::HIDE_LOG_PREFIX)] ? "" : "\033[0;90m[LOG]\033[0m ";
        std::cout << join_string(prefix, args...) << std::endl;
    }

    // Print the message ends with a newline, buffer flushed
    template <typename... Args>
    void print(Args... args) const {
        std::cout << join_string(args...) << std::endl;
    }

    // Print the message with a newline, buffer flushed
    template <typename... Args>
    void newline(Args... args) const {
        std::cout << end_line << join_string(args...) << std::flush;
    }

    // Print the message (no newline), buffer flushed
    template <typename... Args>
    void instant(Args... args) const {
        std::cout << join_string(args...) << std::flush;
    }

    // Add the message to the ostream buffer
    template <typename... Args>
    void append_stream(Args... args) const {
        std::cout << join_string(args...);
    }

    // Flush the output buffer
    void flush() const { std::cout.flush(); }
};
