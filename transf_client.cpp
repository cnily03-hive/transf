#include <WS2tcpip.h>
#include <psdk_inc/_socket_types.h>
#include <winsock2.h>
#include <ws2ipdef.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

// wingdi.h has defined `ERROR` macro, which is conflicting with enum `Level::ERROR`
#ifdef ERROR
#undef ERROR
#endif
#include "ansi.h"
#include "arguments.h"
#include "logger.h"
#include "network.h"
#include "utils.h"

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

Logger logger("Tranf Client");

std::string extract_fn(std::string path) {
    size_t pos = path.find_last_of("\\/");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

int opt_chunk_size = 2048;

#define UUID_LEN 36

#define HEAD_HELLO "\013HELLO"
#define HEAD_HS "\013HS"
#define HEAD_TRANSFER "\013TRANSFER"
#define HEAD_OK "\013OK"
#define HEAD_RECEIVED "\013RECEIVED"
#define HEAD_DONE "\013DONE"
#define HEAD_REJECT "\013REJECT"
#define HEAD_DROP "\013DROP"

#define headcmp(buf, head) (memcmp(buf, head, strlen(head)) == 0)

int handle_hello(SocketClient& remote, char* buf, int buf_size) {
    // Hello
    int err = remote.send(HEAD_HELLO);
    if (err == SOCKET_ERROR) return -1;
    logger.debug("Hello sent");
    int size = remote.recv(buf, buf_size);
    if (size <= 0) return -1;
    return 0;
}

bool check_alive(SocketClient& remote, char* buf, int buf_size, int retry = 1) {
    for (int i = 0; i < retry; i++) {
        logger.debug(i == 0 ? "Connecting" : "Reconnecting");
        int err = handle_hello(remote, buf, opt_chunk_size);
        if (err == 0) {
            return true;
        } else {
            logger.debug("Sent faild (code ", err, ")");
        }
        remote.reconnect();
    }
    return false;
}

int send_file(SocketClient& remote, const std::string& fp, char* buf, int buf_size) {
    int size = 0, err = 0;

    // Prepare file
    std::string fn = extract_fn(fp);
    std::ifstream fs(fp, std::ios::binary);
    // check if file exists
    if (!fs.is_open()) return logger.error("File not found: ", ansi::gray, fp, ansi::reset), -1;

    // Handshake
    logger.debug("[START] Handshake");

    fs.seekg(0, std::ios::end);
    std::streamsize file_size = fs.tellg();
    u_long file_size_net = htonl(file_size);  // convert to network byte order
    memcpy(buf, HEAD_HS, strlen(HEAD_HS));
    memcpy(buf + strlen(HEAD_HS), &file_size_net, sizeof(u_long));
    memcpy(buf + strlen(HEAD_HS) + sizeof(u_long), fn.c_str(), fn.size());
    err = remote.send(buf, strlen(HEAD_HS) + sizeof(u_long) + fn.size());
    if (err == SOCKET_ERROR) return logger.error("Cannot connect to server"), fs.close(), -1;
    // response
    size = remote.recv(buf, buf_size);
    if (size <= 0) return logger.error("Cannot connect to server"), fs.close(), -1;
    if (!headcmp(buf, HEAD_OK)) return logger.error("Handshake failed"), fs.close(), 1;
    const std::string uuid = std::string(buf + strlen(HEAD_OK), UUID_LEN);

    // Transfer
    logger.debug("[START] Transfer");

    u_long chunk = 0;
    u_long send_buf_size = buf_size - strlen(HEAD_TRANSFER) - UUID_LEN - sizeof(u_long);
    u_long tot_chunk = file_size / send_buf_size + (file_size % send_buf_size != 0);
    u_long last_rate = 0;
    fs.seekg(0, std::ios::beg);

    while (!fs.eof()) {
        ++chunk;
        const bool IS_DEBUG = logger.get_level() <= Logger::Level::DEBUG;
        // output
        const std::string ANSI_PREV_LINE =
            ansi::clear_line + ansi::cursor_prev_line(1) + ansi::clear_line;
        u_long rate = chunk == tot_chunk ? 100 : chunk * 100 / tot_chunk;
        if (IS_DEBUG) {
            logger.print(ansi::rgb_fg(0, 0, 139), "  Sending (", rate, "%, chunk ", chunk, "/",
                         tot_chunk, ")", ansi::reset);
        } else if (chunk == 1 || chunk == tot_chunk || last_rate != rate) {
            last_rate = rate;
            logger.print(chunk == 1 ? "" : ANSI_PREV_LINE, ansi::rgb_fg(0, 0, 139), "  Sending (",
                         rate, "%)", ansi::reset);
        }

        auto print_fail = [&fp, &IS_DEBUG]() {
            logger.print(IS_DEBUG ? ""
                                  : ansi::clear_line + ansi::cursor_prev_line(1) +
                                        ansi::cursor_pos_x(fp.size() + 6),
                         ansi::rgb_fg(139, 0, 0), "  (Failed)", ansi::reset);
        };
        auto print_success = [&fp, &IS_DEBUG]() {
            logger.print(IS_DEBUG
                             ? ""
                             : ansi::clear_line + ansi::cursor_prev_line(1) + ansi::clear_line +
                                   ansi::cursor_prev_line(1) + ansi::cursor_pos_x(fp.size() + 6),
                         ansi::rgb_fg(0, 139, 0), "  (Sent)", ansi::reset);
        };

        // send file content
        int chunk_net = htonl(chunk);
        memcpy(buf, HEAD_TRANSFER, strlen(HEAD_TRANSFER));
        memcpy(buf + strlen(HEAD_TRANSFER), uuid.c_str(), UUID_LEN);
        int chunk_offset = strlen(HEAD_TRANSFER) + UUID_LEN;
        memcpy(buf + chunk_offset, &chunk_net, sizeof(chunk_net));
        fs.read(buf + chunk_offset + sizeof(chunk_net), send_buf_size);
        int read_size = fs.gcount();
        err = remote.send(buf, chunk_offset + sizeof(chunk_net) + read_size);
        if (err == SOCKET_ERROR) return print_fail(), fs.close(), 1;

        // receive status (chunk)
        size = remote.recv(buf, buf_size);
        if (size <= 0) return print_fail(), fs.close(), 1;
        if (headcmp(buf, HEAD_RECEIVED)) {
            auto _uuid = std::string(buf + strlen(HEAD_RECEIVED), UUID_LEN);
            if (uuid != _uuid) return print_fail(), fs.close(), 1;
            u_long chunk_recv;
            memcpy(&chunk_recv, buf + strlen(HEAD_RECEIVED) + UUID_LEN, sizeof(u_long));
            chunk_recv = ntohl(chunk_recv);
            if (chunk_recv != chunk + 1) return print_fail(), fs.close(), 1;
        } else if (headcmp(buf, HEAD_DONE)) {
            auto _uuid = std::string(buf + strlen(HEAD_DONE), UUID_LEN);
            if (uuid != _uuid) return print_fail(), fs.close(), 1;
            u_long chunk_recv;
            memcpy(&chunk_recv, buf + strlen(HEAD_DONE) + UUID_LEN, sizeof(u_long));
            chunk_recv = ntohl(chunk_recv);
            if (chunk_recv != chunk + 1) return print_fail(), fs.close(), 1;
            print_success();
            break;
        } else {
            return print_fail(), fs.close(), 1;
        }
    }

    fs.close();
    return 0;
}
#undef headcmp

struct CLIOptions {
    std::string ip;
    int port;
    ip_version ip_ver = IPv4 | IPv6;
    SockType socktype = SockType::TYPE_DGRAM;
    int chunk_size = 2048;
    int timeout_recv = 10000;
    int timeout_send = 10000;
    bool ping = false;
};

inline void cli_usage() {
    logger.print(
        "Usage: transf_client <ip> <port> [options]\n"
        "\n"
        "Options:\n"
        "  -h, --help               Display this help message\n"
        "  --ping                   Ping the server\n"
        "  --protocol <protocol>    Specify the protocol to use (default: udp)\n"
        "  --tcp                    Equivalent to --protocol tcp\n"
        "  --udp                    Equivalent to --protocol udp\n"
        "  --chunk <chunk_size>     Set chunk size for file transfer (default: 2048)\n"
        "  --timeout <timeout>      Set timeout for sending and receiving data (default: 10000)\n"
        "  --debug                  Enable debug mode\n"
        "\n"
        "Copyright (c) 2024 Jevon Wang, MIT License\n"
        "Source code: github.com/cnily03-hive/transf");
}

int main(int argc, char* argv[]) {
    int err = -1;  // global error code

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
        } else if (arg_match(cur_argstr, "--ping")) {
            // opt: --ping
            options.ping = true;
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

    opt_chunk_size = options.chunk_size;

    // End processing arguments

    if (options.ping) logger.set_level(Logger::Level::INFO);

    if (p1 == nullptr || p2 == nullptr) return cli_usage(), 1;
    options.ip = p1;
    options.port = std::stoi(p2);
    p1 = p2 = nullptr;

    // check arguments
    if (!check_ip(options.ip)) {
        return logger.error("Invalid IP address: ", options.ip), 1;
    }
    if (!check_port(options.port)) {
        return logger.error("Invalid port: ", options.port), 1;
    }
    options.ip_ver = get_ipversion(options.ip);

    // [debug] print options info
    logger.debug("Options informaton:");
    if (logger.get_level() <= Logger::Level::DEBUG) {
        logger.print(" - IP: ", options.ip);
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
    err = ip_addrcoll(options.ip, options.port, &hints, &ipinfo);
    if (err != 0)
        return logger.error("Failed to parse ip address (", err, ")"), BasicSocket::terminate(), 1;

    // Create socket
    logger.debug("Creating socket");
    SocketClient client((BasicSocket(ipinfo)));
    // init
    err = client.init_socket();
    if (err != 0)
        return logger.error("Failed to create socket (", err, ")"), BasicSocket::terminate(), 1;
    // set opt
    const int SND_TIMEOUT = options.timeout_send;
    const int RCV_TIMEOUT = options.timeout_recv;
    int e1, e2;
    e1 = setsockopt(client.socket(), SOL_SOCKET, SO_SNDTIMEO, (const char*)&SND_TIMEOUT,
                    sizeof(SND_TIMEOUT));
    e2 = setsockopt(client.socket(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&RCV_TIMEOUT,
                    sizeof(RCV_TIMEOUT));
    if (e1 == SOCKET_ERROR || e2 == SOCKET_ERROR) {
        return logger.error("Failed to set socket options"), BasicSocket::terminate(), 1;
    }
    // check once more
    if (!client.ensure_socket())
        return logger.error("Socket is not active"), BasicSocket::terminate(), 1;

    // cache
    char* buf = new char[opt_chunk_size];
    // ZeroMemory(buf, opt_chunk_size);

    if (options.ping) {
        // ping, only send hello
        auto conn = client.conn_info();
        std::string address = conn.to_string();
        std::string type = conn.type == TYPE_STREAM ? "TCP" : TYPE_DGRAM ? "UDP" : "IP";
        logger.print("PING ", address, " ", type, " ...");
        int maxtry = 4;
        for (int i = 0; i < maxtry; i++) {
            client.connect();
            auto start = Timer::point();
            bool alive = check_alive(client, buf, opt_chunk_size, 1);
            auto end = Timer::point();
            std::string ms = std::to_string(Timer::duration(start, end));
            if (alive) {
                logger.print("Hello from ", address, " ", type, ": time=", ms, " ms");
            } else {
                logger.print("Cannot connect to server");
            }
            if (i + 1 < maxtry) Timer::sleep(1000);
        }

        // Clean up
        delete[] buf;
        client.close();
        client.destroy();
        BasicSocket::terminate();
        return 0;
    }

    // everything goes well
    client.connect();
    if (!check_alive(client, buf, opt_chunk_size, 3)) {
        return logger.error("Cannot connect to server"), 1;
    } else {
        logger.debug("Connceted");
    }

    // Interactive
    bool exit = false;
    logger.print(ansi::cyan, "Type a file path to send, or type \"@exit\" to exit", ansi::reset);
    while (!exit) {
        std::string input;
        logger.instant(ansi::bright_cyan, "> ", ansi::reset);
        getline(std::cin, input);
        input = trim(input);

        if (input.size() > 0 && !input.starts_with("@")) {
            std::string fore_str = join_string(ansi::bright_cyan, "> ", ansi::reset, input);
            std::string fp = input;

            if (!check_alive(client, buf, opt_chunk_size, 3)) {
                logger.error("Cannot connect to server");
                continue;
            }

            err = send_file(client, fp, buf, opt_chunk_size);

        } else if (input.size() > 0 && input.starts_with("@")) {
            if (input == "@exit" || input == "@quit" || input == "@q") {
                exit = true;
            } else if (input == "@") {
                logger.info("Empty command");
            } else {
                logger.error("Unkown command: ", input.substr(1));
            }
        }
    }

    // Clean up
    delete[] buf;
    client.close();
    client.destroy();
    BasicSocket::terminate();

    return 0;
}
