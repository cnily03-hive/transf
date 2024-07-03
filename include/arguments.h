#ifndef __ARGUMENTS_H__
#define __ARGUMENTS_H__

#if __cplusplus < 201703L
#include <iterator>
#endif

#include <string>

class ArgsLoop {
   private:
    const int argc;
    const char *const *argv;
    const char *const exe_path;
    int index;

   public:
    ArgsLoop(const ArgsLoop &) = default;
    ArgsLoop(ArgsLoop &&) = default;

    ArgsLoop(int argc, const char *const *argv);

    const char *prev();
    const char *next();
    const char *get(int i = 0);
    const char *read(int i);
    const char *current();
    int get_index();
    void set_index(int i);
    void reset();
    const char *executable();

    bool has_index(int i);
    bool has_next();
    bool has_prev();
    bool is_end();
    bool is_begin();

    const char *operator[](int i);
    ArgsLoop &operator++();
    ArgsLoop &operator++(int);
    ArgsLoop &operator--();
    ArgsLoop &operator--(int);
    ArgsLoop &operator=(const ArgsLoop &) = delete;
    ArgsLoop &operator=(ArgsLoop &&) = delete;

#if __cplusplus < 201703L
   public:
    /**
     * @deprecated C++17 does not recommend using iterator, and it's marked as deprecated.
     */
    class iterator : std::iterator<std::input_iterator_tag, const char *> {
        ArgsLoop *loop;
        int index;

       public:
        iterator(ArgsLoop *loop, int index);
        iterator(const iterator &it);

        iterator &operator++();
        iterator operator++(int);

        bool operator==(const iterator &it) const;
        bool operator!=(const iterator &it) const;
        const char *operator*();
    };
#endif
};

bool arg_match(const std::string &src, std::initializer_list<std::string> targets);
bool arg_match(const std::string &src, const std::string &t1);
bool arg_match(const std::string &src, const std::string &t1, const std::string &t2);
bool arg_match(const std::string &src, const std::string &t1, const std::string &t2,
               const std::string &t3);
bool arg_match(const std::string &src, const std::string &t1, const std::string &t2,
               const std::string &t3, const std::string &t4);
#endif  // __ARGUMENTS_H__