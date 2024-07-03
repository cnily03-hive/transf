#include "arguments.h"

ArgsLoop::ArgsLoop(int argc, const char *const *argv)
    : argc(argc), argv(argv), exe_path(argv[0]), index(1) {}

const char *ArgsLoop::prev() {
    if (index < 0) return nullptr;
    --index;
    if (index < 0) return nullptr;
    return argv[index];
}

const char *ArgsLoop::next() {
    if (index >= argc) return nullptr;
    ++index;
    if (index >= argc) return nullptr;
    return argv[index];
}

const char *ArgsLoop::get(int i) { return read(index + i); }

const char *ArgsLoop::read(int i) {
    if (i < 0 || i >= argc) return nullptr;
    return argv[i];
}

const char *ArgsLoop::current() {
    if (index < 0 || index >= argc) return nullptr;
    return argv[index];
}

int ArgsLoop::get_index() { return index; }

void ArgsLoop::set_index(int i) {
    if (i < 0)
        index = -1;
    else if (i >= argc)
        index = argc;
    else
        index = i;
}

void ArgsLoop::reset() { index = 1; }
const char *ArgsLoop::executable() { return exe_path; }

bool ArgsLoop::has_index(int i) { return i >= 0 && i < argc; }
bool ArgsLoop::has_next() { return index + 1 < argc; }
bool ArgsLoop::has_prev() { return index - 1 >= 0; }
bool ArgsLoop::is_end() { return index >= argc; }
bool ArgsLoop::is_begin() { return index < 0; }

const char *ArgsLoop::operator[](int i) { return read(i); }
ArgsLoop &ArgsLoop::operator++() { return next(), *this; }
ArgsLoop &ArgsLoop::operator++(int) { return next(), *this; }
ArgsLoop &ArgsLoop::operator--() { return prev(), *this; }
ArgsLoop &ArgsLoop::operator--(int) { return prev(), *this; }

#if __cplusplus < 201703L
/**
 * @deprecated C++17 does not recommend using iterator, and it's marked as deprecated.
 */
class iterator : std::iterator<std::input_iterator_tag, const char *> {
    ArgsLoop *loop;
    int index;

   public:
    ArgsLoop::iterator::iterator(ArgsLoop *loop, int index) : loop(loop), index(index) {}
    ArgsLoop::iterator::iterator(const iterator &it) : loop(it.loop), index(it.index) {}

    ArgsLoop::iterator &ArgsLoop::iterator::operator++() {
        ++index;
        return *this;
    }

    ArgsLoop::iterator ArgsLoop::iterator::operator++(int) {
        iterator it = *this;
        ++index;
        return it;
    }

    bool ArgsLoop::iterator::operator==(const iterator &it) const { return index == it.index; }
    bool ArgsLoop::iterator::operator!=(const iterator &it) const { return index != it.index; }
    const char *ArgsLoop::iterator::operator*() { return loop->read(index); }
};
#endif

bool arg_match(const std::string &src, std::initializer_list<std::string> targets) {
    for (const auto &target : targets) {
        if (src == target) return true;
    }
    return false;
}

bool arg_match(const std::string &src, const std::string &t1) { return src == t1; }
bool arg_match(const std::string &src, const std::string &t1, const std::string &t2) {
    return src == t1 || src == t2;
}
bool arg_match(const std::string &src, const std::string &t1, const std::string &t2,
               const std::string &t3) {
    return src == t1 || src == t2 || src == t3;
}
bool arg_match(const std::string &src, const std::string &t1, const std::string &t2,
               const std::string &t3, const std::string &t4) {
    return src == t1 || src == t2 || src == t3 || src == t4;
}