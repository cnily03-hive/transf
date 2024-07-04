#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ansi.h"

// wingdi.h has defined `ERROR` macro, which is conflicting with enum `Level::ERROR`
#ifdef ERROR
#undef ERROR
#endif
#include "arguments.h"
#include "logger.h"
#include "network.h"

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

Logger logger("Tranf Server");

#define UUID_LEN 36

#define HEAD_HELLO "\013HELLO"
#define HEAD_HS "\013HS"
#define HEAD_TRANSFER "\013TRANSFER"
#define HEAD_OK "\013OK"
#define HEAD_RECEIVED "\013RECEIVED"
#define HEAD_DONE "\013DONE"
#define HEAD_REJECT "\013REJECT"
#define HEAD_DROP "\013DROP"

enum struct TransferStatus {
    HANDSHAKE,
    TRANSFERING,
    DONE,
};

std::string opt_abs_save_path = "";

struct TransferInfo {
    TransferStatus status;
    std::string filename;
    u_long filesize;
    std::string abs_fp;
    u_long written;
    std::ofstream* fs;
    u_long chunk;
    int last_update_time;
    std::mutex* mutex;
    int update_time() { return last_update_time = Timer::timestamp(); }
    bool use() {
        bool is_lock = mutex->try_lock();
        update_time();
        return is_lock;
    }
    bool unuse() {
        update_time();
        mutex->unlock();
        return true;
    }
};

std::map<std::string, TransferInfo> file_transfer_info;

std::mutex file_transfer_info_mutex;

bool need_cleanup = false;

void cleanup_expired_file_transfer_info(int live_time, int check_interval) {
    while (need_cleanup) {
        logger.debug(("[Cleanup] Check start"));
        UniqueLock lockmap(file_transfer_info_mutex);
        int curtime = Timer::timestamp();
        for (auto it = file_transfer_info.begin(); it != file_transfer_info.end();) {
            lockmap.unlock();
            std::string uuid = it->first;
            TransferInfo& info = it->second;
            auto pmutex = info.mutex;
            UniqueLock lock(*pmutex, std::try_to_lock);
            // still alive
            if (!lock.owns_lock() && info.last_update_time + live_time > curtime) {
                lockmap.lock();
                ++it;
                continue;
            }
            // not alive
            logger.debug("[Cleanup] Cleaning expired file transfer: ", uuid);
            if (info.fs != nullptr) {
                info.fs->close();
                delete info.fs;
                info.fs = nullptr;
            }
            // remove file object if exists
            if (std::filesystem::exists(info.abs_fp)) {
                bool success_remove = false;
                try {
                    success_remove = std::filesystem::remove(info.abs_fp.c_str());
                } catch (std::filesystem::filesystem_error& e) {
                    int prefixlen =
                        ansi::remove_ansi(logger.get_colored_prefix(Logger::Level::ERROR)).size();
                    logger.error("[Cleanup] Failed to remove file: ", ansi::gray, info.abs_fp,
                                 ansi::reset);
                    logger.level_print(Logger::Level::ERROR, std::string(prefixlen, ' '), e.what());
                }
                if (!success_remove) {
                    logger.error("[Cleanup] Failed to remove file: ", ansi::gray, info.abs_fp,
                                 ansi::reset);
                } else {
                    logger.debug("[Cleanup] File removed: ", ansi::gray, info.abs_fp, ansi::reset);
                }
            }
            // remove from map
            lockmap.lock();
            it = file_transfer_info.erase(it);
            lock.unlock();
            delete pmutex;
        }
        lockmap.unlock();
        logger.debug("[Cleanup] Check sleep");
        Timer::sleep(check_interval);
    }
}

#define headcmp(buf, head) memcmp(buf, head, strlen(head)) == 0

int handle_hello(const char* buf, int len, const SocketPeer& peer, const BasicSocket& server) {
    auto address = peer.conn_info().to_string(true);

    if (headcmp(buf, HEAD_HELLO)) {
        logger.debug(address, " - ", "Hello");
        peer.send(HEAD_HELLO);
        return HANDLE_END;
    }

    return HANDLE_NEXT;
}

int handle_file_transfer(const char* buf, int len, const SocketPeer& peer,
                         const BasicSocket& server) {
    auto address = peer.conn_info().to_string(true);

    const bool IS_DEBUG = logger.get_level() <= Logger::Level::DEBUG;

    // Handshake
    if (headcmp(buf, HEAD_HS)) {
        logger.debug(address, " - ", "Handshake");
        int LEN_HEAD = strlen(HEAD_HS);

        u_long file_size;
        memcpy(&file_size, buf + LEN_HEAD, sizeof(u_long));
        file_size = ntohl(file_size);
        std::string fn(buf + LEN_HEAD + sizeof(u_long), len - LEN_HEAD - sizeof(u_long));

        // check filename (starts with /, or contains .., or empty)
        if (fn.size() == 0 || fn[0] == '/' || fn.find("..") != std::string::npos) {
            logger.info(address, " - ", "Refused to receive file: ", fn);
            // send reject
            return peer.send(HEAD_REJECT), HANDLE_END;
        }

        // create directory
        std::filesystem::path save_dir_abspath(opt_abs_save_path);
        int create_dir_err = CreateDirectory(save_dir_abspath.string().c_str(), NULL);
        int is_already_exists = create_dir_err == 0 && GetLastError() == ERROR_ALREADY_EXISTS;
        bool dir_err = create_dir_err == 0 && !is_already_exists;
        // create file
        std::string save_fp_str = (save_dir_abspath / fn).string();
        std::ofstream* pfs = new std::ofstream(save_fp_str, std::ios::binary);
        if (dir_err || !pfs->is_open()) {
            logger.error(address, " - ", "Failed to create file: ", save_fp_str);
            // send drop
            pfs->close();
            delete pfs;
            return peer.send(HEAD_DROP), HANDLE_END;
        }

        logger.info(address, " - ", "Receiving file (", fmt_size(file_size), "): ", ansi::gray, fn,
                    ansi::reset);

        auto uuid = uuid_v1();

        TransferInfo info{
            TransferStatus::HANDSHAKE, fn, file_size, save_fp_str, 0, pfs, 1, Timer::timestamp(),
            new std::mutex()};
        // ! Do not forget to unuse() before return !! Or it will never be used again or removed !!!
        info.use();
        {  // insert to map, it may be deleted if info is not in use
            UniqueLock _lock(file_transfer_info_mutex);
            file_transfer_info.insert({uuid, info});
        }

        // only stream socket will call this onclose
        peer.onclose([uuid](const SocketPeer& peer) -> int {
            logger.info(peer.conn_info().to_string(true), "- Connection closed - ", uuid);
            UniqueLock _lock(file_transfer_info_mutex);
            auto it = file_transfer_info.find(uuid);
            if (it != file_transfer_info.end()) {
                auto pmutex = it->second.mutex;
                TransferInfo& info = it->second;
                if (info.fs != nullptr) {
                    info.fs->close();
                    delete info.fs;
                    info.fs = nullptr;
                }

                // remove file object if exists
                if (std::filesystem::exists(info.abs_fp)) {
                    bool success_remove = false;
                    try {
                        success_remove = std::filesystem::remove(info.abs_fp.c_str());
                    } catch (std::filesystem::filesystem_error& e) {
                        int prefixlen =
                            ansi::remove_ansi(logger.get_colored_prefix(Logger::Level::ERROR))
                                .size();
                        logger.error("Failed to remove file: ", ansi::gray, info.abs_fp,
                                     ansi::reset);
                        logger.level_print(Logger::Level::ERROR, std::string(prefixlen, ' '),
                                           e.what());
                    }
                    if (!success_remove) {
                        logger.error("Failed to remove file: ", ansi::gray, info.abs_fp,
                                     ansi::reset);
                    } else {
                        logger.debug("File removed: ", ansi::gray, info.abs_fp, ansi::reset);
                    }
                }

                file_transfer_info.erase(uuid);
                pmutex->unlock();
                delete pmutex;
            }
            return 0;
        });

        peer.send(HEAD_OK + uuid);
        return info.unuse(), HANDLE_END;

    }

    // Transfer
    else if (headcmp(buf, HEAD_TRANSFER)) {
        logger.debug(address, " - ", "Transfering");
        int LEN_HEAD = strlen(HEAD_TRANSFER);

        // check uuid
        std::string uuid(buf + LEN_HEAD, UUID_LEN);
        if (IS_DEBUG) logger.instant(ansi::cursor_prev_line(1) + ansi::clear_line);
        logger.debug(address, " - ", "Transfering - ", uuid);

        UniqueLock lockmap(file_transfer_info_mutex);  // lock global map
        auto it = file_transfer_info.find(uuid);
        if (it == file_transfer_info.end()) return peer.send(HEAD_REJECT), HANDLE_END;

        TransferInfo& info = it->second;
        if (!info.use()) return info.unuse(), peer.send(HEAD_REJECT), HANDLE_END;
        lockmap.unlock();

        // verify chunk
        u_long chunk;
        memcpy(&chunk, buf + LEN_HEAD + UUID_LEN, sizeof(u_long));
        chunk = ntohl(chunk);
        if (IS_DEBUG) logger.instant(ansi::cursor_prev_line(1) + ansi::clear_line);
        logger.debug(address, " - ", "Transfering - ", uuid, " - ", chunk, "/", info.chunk);
        if (chunk != info.chunk) return info.unuse(), peer.send(HEAD_REJECT), HANDLE_END;

        auto& fs = *info.fs;

        int this_written = len - LEN_HEAD - UUID_LEN - sizeof(u_long);
        if (this_written <= 0)
            this_written = 0;
        else if (info.filesize <= info.written)
            this_written = 0;
        else if (info.filesize < info.written + this_written)
            this_written = info.filesize - info.written;

        if (this_written > 0) {
            if (!fs.is_open()) return info.unuse(), peer.send(HEAD_DROP), HANDLE_END;
            fs.write(buf + LEN_HEAD + UUID_LEN + sizeof(u_long), this_written);
            // info.written = fs.tellp();
            info.written += this_written;
            if (IS_DEBUG) logger.instant(ansi::cursor_prev_line(1) + ansi::clear_line);
            logger.debug(address, " - ", "Transfering - ", uuid, " - ", chunk, "/", info.chunk,
                         " - add ", info.written, "/", info.filesize, " bytes");
            ++info.chunk;
        }
        u_long chunk_net = htonl(info.chunk);
        if (info.written >= info.filesize) {
            fs.close();
            delete info.fs;
            info.fs = nullptr;
            logger.info(address, " - ", "File received (", fmt_size(info.filesize),
                        "): ", info.filename);
            int buflen = strlen(HEAD_DONE) + UUID_LEN + sizeof(u_long);
            char* buf = new char[buflen];
            memcpy(buf, HEAD_DONE, strlen(HEAD_DONE));
            memcpy(buf + strlen(HEAD_DONE), uuid.c_str(), UUID_LEN);
            memcpy(buf + strlen(HEAD_DONE) + UUID_LEN, &chunk_net, sizeof(u_long));
            peer.send(buf, buflen);
            delete[] buf;
            lockmap.lock();
            auto pmutex = info.mutex;
            file_transfer_info.erase(uuid);
            pmutex->unlock();
            delete pmutex;
            lockmap.unlock();
            peer.end();
            return HANDLE_END;
        } else {
            int buflen = strlen(HEAD_RECEIVED) + UUID_LEN + sizeof(u_long);
            char* buf = new char[buflen];
            memcpy(buf, HEAD_RECEIVED, strlen(HEAD_RECEIVED));
            memcpy(buf + strlen(HEAD_RECEIVED), uuid.c_str(), UUID_LEN);
            memcpy(buf + strlen(HEAD_RECEIVED) + UUID_LEN, &chunk_net, sizeof(u_long));
            peer.send(buf, buflen);
            delete[] buf;
            return info.unuse(), HANDLE_END;
        }
    }

    return HANDLE_NEXT;
}

#undef headcmp

struct CLIOptions {
    bool has_opt_ip = false;
    std::string ip = "";  // any
    int port;
    ip_version ip_ver = IPv4 | IPv6;
    SockType socktype = SockType::TYPE_DGRAM;
    std::string save_path = "./received";
    int chunk_size = 2048;
    int timeout_recv = 10000;
    int timeout_send = 10000;
    bool listen_all = false;
};

inline void cli_usage() {
    logger.print(
        "Usage: transf_server [ip] <port> [options]\n"
        "\n"
        "Options:\n"
        "  -h, --help               Display this help message\n"
        "  -d, --dir <path>         Save received files to the specified directory\n"
        "  --protocol <protocol>    Specify the protocol to use (default: udp)\n"
        "  --tcp                    Equivalent to --protocol tcp\n"
        "  --udp                    Equivalent to --protocol udp\n"
        "  --chunk <size>           Set chunk size for file transfer (default: 2048)\n"
        "  --timeout <timeout>      Set timeout for sending and receiving data (default: "
        "10000)\n"
        "  --debug                  Enable debug mode\n"
        "  --listen-all             Listen on all available interfaces\n"
        "\n"
        "Copyright (c) 2024 Jevon Wang, MIT License\n"
        "Source code: github.com/cnily03-hive/transf");
}

int main(int argc, char* argv[]) {
    int err = -1;  // global error code

    logger.set_color(Logger::Level::INFO, ansi::bright_cyan);
    logger.set_level(Logger::Level::INFO);

    //===--------------------------------------------------===//
    // Parse command line arguments
    //===--------------------------------------------------===//

    CLIOptions options;
    ArgsLoop args(argc, argv);
    const char *p1 = nullptr, *p2 = nullptr;
    while (!args.is_end()) {
        const std::string cur_argstr = args.current();
        // const int index = args.get_index();

        if (arg_match(cur_argstr, "--help", "-h")) {
            // opt: --help
            return cli_usage(), 0;
        } else if (arg_match(cur_argstr, "--debug")) {
            // opt: --debug
            logger.set_level(Logger::Level::DEBUG);
        } else if (arg_match(cur_argstr, "--dir", "-d")) {
            // opt: --dir
            auto next = args.next();
            if (next == nullptr) {
                return logger.error("Missing argument for --dir"), 1;
            }
            options.save_path = std::string(next);
            try {
                auto t = std::filesystem::canonical(options.save_path).string();
            } catch (...) {
                return logger.error("Invalid path: ", options.save_path), 1;
            }
        } else if (arg_match(cur_argstr, "--protocol")) {
            // opt: --protocol
            auto next = args.next();
            if (next == nullptr) {
                return logger.error("Missing argument for --protocol"), 1;
            }
            if (strcmp(next, "udp") == 0) {
                options.socktype = SockType::TYPE_DGRAM;
            } else if (strcmp(next, "tcp") == 0) {
                options.socktype = SockType::TYPE_STREAM;
            } else {
                return logger.error("Invalid protocol: ", next), 1;
            }
        } else if (arg_match(cur_argstr, "--tcp")) {
            // opt: --tcp
            options.socktype = SockType::TYPE_STREAM;
        } else if (arg_match(cur_argstr, "--udp")) {
            // opt: --udp
            options.socktype = SockType::TYPE_DGRAM;
        } else if (arg_match(cur_argstr, "--chunk")) {
            // opt: --chunk
            auto next = args.next();
            if (next == nullptr) {
                return logger.error("Missing argument for --chunk"), 1;
            }
            try {
                int chunk_size = std::stoi(next);
                if (chunk_size <= 0)
                    return logger.error(
                               "Invalid argument: "
                               "chunk size must be a positive integer: ",
                               next),
                           1;
                options.chunk_size = chunk_size;
            } catch (...) {
                return logger.error("Invalid chunk size: ", next), 1;
            }
        } else if (arg_match(cur_argstr, "--timeout")) {
            // opt: --timeout
            auto next = args.next();
            if (next == nullptr) {
                return logger.error("Missing argument for --timeout"), 1;
            }
            try {
                int timeout = std::stoi(next);
                if (timeout <= 0)
                    return logger.error(
                               "Invalid argument: "
                               "timeout must be a positive integer: ",
                               next),
                           1;
                options.timeout_recv = options.timeout_send = timeout;
            } catch (...) {
                return logger.error("Invalid timeout: ", next), 1;
            }
        } else if (arg_match(cur_argstr, "--listen-all")) {
            options.listen_all = true;
        } else if (cur_argstr.starts_with("--")) {
            // long options
            return logger.error("Unknown option: ", cur_argstr), 1;
        } else if (cur_argstr.starts_with("-")) {
            // short options
            return logger.error("Unknown option: ", cur_argstr), 1;
        } else {
            // simple arguments
            if (p1 == nullptr) {
                p1 = args.current();
            } else if (p2 == nullptr) {
                p2 = args.current();
            } else {
                return logger.error("Unknown argument: ", cur_argstr), 1;
            }
        }

        args.next();
    }

    if (p1 == nullptr) {
        return cli_usage(), 1;
    } else if (p2 == nullptr) {
        // only has port
        options.has_opt_ip = false;
        options.ip = "";  // any, auto detect ip addresses available on all interfaces
        options.ip_ver = IPv4 | IPv6;
        if (!check_port(p1)) {
            return logger.error("Invalid port: ", p1), 1;
        }
        options.port = std::stoi(p1);
    } else {
        // specified ip and port
        options.has_opt_ip = true;
        if (!check_port(p2)) {
            return logger.error("Invalid port: ", p2), 1;
        }
        options.port = std::stoi(p2);
        if (options.listen_all) {
            options.ip = "";
            options.ip_ver = IPv4 | IPv6;
        } else {
            options.ip_ver = get_ipversion(p1);
            if (options.ip_ver == 0) {
                return logger.error("Invalid IP address: ", p1), 1;
            }
            options.ip = p1;
        }
    }

    p1 = p2 = nullptr;

    // parse absolute path
    if (options.save_path.empty()) {
        return logger.error("Undefined save path, use --dir to specify."), 1;
    }
    try {
        opt_abs_save_path = std::filesystem::canonical(options.save_path).string();
    } catch (...) {
        logger.error("An error occurred while parsing the save path: ", options.save_path);
        return 1;
    }
    // End of parsing command line arguments
    // it's ensured that `options.ip` is a valid ipaddress or empty string ("")
    // `options.ip_ver` must be IPv4 or IPv6 or both
    // `options.ip_ver` must be both when options.ip is "", which means all address (except
    // loopback)

    if (logger.get_level() <= Logger::Level::DEBUG)
        logger.info(ansi::magenta, "Debug mode: ", ansi::green, ansi::bold, "ON", ansi::reset);

    // [debug] print options info
    logger.debug("Options informaton:");
    if (logger.get_level() <= Logger::Level::DEBUG) {
        logger.print(" - IP: ", options.ip.empty() ? "(any)" : options.ip);
        logger.print(" - Port: ", options.port);
        logger.print(" - Protocol: ", options.socktype == SockType::TYPE_DGRAM    ? "UDP"
                                      : options.socktype == SockType::TYPE_STREAM ? "TCP"
                                                                                  : "Unknown");
        logger.print(" - IP Version: ", options.ip_ver == IPv6   ? "IPv6"
                                        : options.ip_ver == IPv4 ? "IPv4"
                                                                 : "IPv4, IPv6");
        logger.print(" - Chunk Size: ", options.chunk_size, " Bytes");
        logger.print(" - Timeout (Recv): ", options.timeout_recv);
        logger.print(" - Timeout (Send): ", options.timeout_send);
        logger.print(" - Save path: ", options.save_path);
    }

    //===--------------------------------------------------===//
    // Socket initialization
    //===--------------------------------------------------===//

    // Init winsock
    logger.debug("Initializing Winsock");
    WSADATA wsa_data;
    err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (err != 0) return logger.error("WSAStartup (", err, ")"), WSACleanup(), 1;

    // Parse ip address
    logger.debug("Parsing ip address");
    // hints, no specific address information
    addrhint hints = get_hints(options.ip_ver, options.socktype);
    // parsed information of ip address(es)
    addrcoll* ipinfo = nullptr;
    if (options.has_opt_ip || options.listen_all) {
        // `ip_addrcoll` is used to inject ip and port from a hint to a socket
        // if ip is "", it will automatically bind to all available ip addresses
        // however, it may not include ip address(es) on loopback interface
        err = ip_addrcoll(options.ip, options.port, &hints, &ipinfo);
        if (err != 0)
            return logger.error("Failed to parse ip address (", err, ")"), WSACleanup(), 1;
    }
    if (!options.has_opt_ip || options.listen_all) {
        // if user does not specify ip address, we need append loopback address to the results
        // list
        addrcoll* loopback_info_udp = nullptr;
        // nodename is `NULL`, meaning all address
        err = ip_addrcoll(NULL, options.port, &hints, &loopback_info_udp);
        if (err == 0) {
            addrcoll* p = loopback_info_udp;
            while (p->ai_next != nullptr) p = p->ai_next;
            p->ai_next = ipinfo;
            ipinfo = loopback_info_udp;
        }
    }

    // Create server socket(s)
    logger.debug("Creating server socket(s)");
    std::list<SocketServer> servers;
    bool err_bindport = false;
    for (addrcoll* paddr = ipinfo; paddr != nullptr; paddr = paddr->ai_next) {
        servers.emplace_back(BasicSocket(paddr));
        SocketServer& server = servers.back();
        std::string address_to_bind = server.conn_info().to_string();

        // init socket
        err = server.init_socket();
        if (err != 0) {
            logger.debug("Failed to create socket for ", address_to_bind, " (", err, ")");
            server.destroy(), servers.pop_back();
            continue;
        }
        // set socket options
        const int RCV_TIMEOUT = options.timeout_recv;
        const int SND_TIMEOUT = options.timeout_send;
        int e1, e2;
        e1 = setsockopt(server.socket(), SOL_SOCKET, SO_RCVTIMEO,  // receive timeout
                        (const char*)&RCV_TIMEOUT, sizeof(RCV_TIMEOUT));
        e2 = setsockopt(server.socket(), SOL_SOCKET, SO_SNDTIMEO,  // send timeout
                        (const char*)&SND_TIMEOUT, sizeof(SND_TIMEOUT));
        if (e1 == SOCKET_ERROR || e2 == SOCKET_ERROR) {
            logger.error("Failed to set socket options (", e1, ", ", e2, ")");
            server.destroy(), servers.pop_back();
            continue;
        }
        // bind address
        err = server.bind_address();
        if (err != 0) {
            logger.debug("Failed to bind socket for ", address_to_bind, " (", err, ")");
            if (err == WSAEADDRINUSE) err_bindport = true;
            server.destroy(), servers.pop_back();
            continue;
        }

        if (!server.ensure()) {
            logger.debug("Socket is not ready for ", address_to_bind);
            server.destroy(), servers.pop_back();
            continue;
        }

        // everything goes well
        server.listen();
    }
    FreeAddrInfo(ipinfo);

    if (servers.empty()) {
        err_bindport ? logger.error("Failed to bind port: port ", options.port, " already in use.")
                     : logger.error("No socket is created or active.");
        return BasicSocket::terminate(), 1;
    }

    logger.append_stream("Server is running, ready on:", END_LINE);
    for (auto& server : servers) {
        logger.append_stream("  ", server.conn_info().to_string(), END_LINE);
    }
    logger.instant("You can access this server by the above address",
                   servers.size() > 1 ? "es" : "", END_LINE);
    logger.print("The file received will be storaged at: ", ansi::gray, opt_abs_save_path,
                 ansi::reset);
    logger.endl();

    std::vector<std::thread> threads;

    need_cleanup = true;
    std::thread cleanup_thread(cleanup_expired_file_transfer_info,
                               int(options.timeout_send + options.timeout_recv),
                               int((options.timeout_send + options.timeout_recv) * 1.5));

    for (auto& server : servers) {
        server.onmessage(&handle_hello);
        server.onmessage(&handle_file_transfer);
        threads.emplace_back(&SocketServer::serve, &server, options.chunk_size);
    }

    // Wait for all threads to finish
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    need_cleanup = false;
    if (cleanup_thread.joinable()) cleanup_thread.join();

    // Clean up
    for (auto& server : servers) {
        server.close();
        server.destroy();
    }
    BasicSocket::terminate();

    return 0;
}
