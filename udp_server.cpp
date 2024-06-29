#include <WS2tcpip.h>
#include <winsock2.h>
#include <ws2ipdef.h>

#include <fstream>
#include <iostream>
#include <string>
#include <thread>

// wingdi.h has defined `ERROR` macro, which is conflicting with enum `Level::ERROR`
#ifdef ERROR
#undef ERROR
#endif
#include "logger.h"

#define ANSI_DELLINE "\033[F\033[K"

#pragma comment(lib, "ws2_32.lib")

#define SAVE_PATH "./received"
#define CHUNK_SIZE 1024

#define SOCKET_SND_TIMEOUT 10000
#define SOCKET_RCV_TIMEOUT 5000

Logger logger("UDP Server");

int bind_ipv4(SOCKET& s, const int port) {
    sockaddr_in ipv4_hint;
    memset(&ipv4_hint, 0, sizeof(ipv4_hint));
    ipv4_hint.sin_family = AF_INET;
    ipv4_hint.sin_addr.s_addr = htonl(INADDR_ANY);
    ipv4_hint.sin_port = htons(port);
    int bind_ok = bind(s, (sockaddr*)&ipv4_hint, sizeof(ipv4_hint));
    return bind_ok;
}

int bind_ipv6(SOCKET& s, const int port) {
    sockaddr_in6 ipv6_hint;
    memset(&ipv6_hint, 0, sizeof(ipv6_hint));
    ipv6_hint.sin6_family = AF_INET6;
    ipv6_hint.sin6_addr = in6addr_any;
    ipv6_hint.sin6_port = htons(port);
    int bind_ok = bind(s, (sockaddr*)&ipv6_hint, sizeof(ipv6_hint));
    return bind_ok;
}

std::string get_ipv4(sockaddr& addr) {
    char remote_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &((sockaddr_in*)&addr)->sin_addr, remote_ip, INET_ADDRSTRLEN);
    return std::string(remote_ip);
}

std::string get_ipv6(sockaddr& addr) {
    char remote_ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &((sockaddr_in6*)&addr)->sin6_addr, remote_ip, INET6_ADDRSTRLEN);
    return std::string(remote_ip);
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

SOCKET create_socket(const int& port, int family) {
    SOCKET s = socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        int err = WSAGetLastError();
        logger.error("Failed to create socket (code: ", err, ")");
        return INVALID_SOCKET;
    }

    // set socket opt
    const int RCV_TIMEOUT = SOCKET_RCV_TIMEOUT;
    const int SND_TIMEOUT = SOCKET_SND_TIMEOUT;
    int opt_ok_1 =
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&RCV_TIMEOUT, sizeof(RCV_TIMEOUT));
    int opt_ok_2 =
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&SND_TIMEOUT, sizeof(SND_TIMEOUT));
    if (opt_ok_1 == SOCKET_ERROR || opt_ok_2 == SOCKET_ERROR) {
        logger.error("Failed to set socket options");
        return INVALID_SOCKET;
    }

    // bind ip and port
    int bind_ok = family == AF_INET ? bind_ipv4(s, port) : bind_ipv6(s, port);
    if (bind_ok != 0) {
        logger.error("Failed to bind port");
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

void handle_socket(SOCKET& s_in, int family) {
    sockaddr_in client4;
    sockaddr_in6 client6;
    sockaddr* client = family == AF_INET ? (sockaddr*)&client4 : (sockaddr*)&client6;
    int client_size = family == AF_INET ? sizeof(client4) : sizeof(client6);

    const int BUFFER_SIZE = CHUNK_SIZE;
    while (true) {
        char buffer[BUFFER_SIZE];
        ZeroMemory(buffer, BUFFER_SIZE);

        int bytes_received = recvfrom(s_in, buffer, BUFFER_SIZE, 0, client, &client_size);
        if (bytes_received == SOCKET_ERROR) continue;

        // -- hello --

        // hello message: \u000AHELLO
        std::string msg_hello = "\u000AHELLO";
        std::string msg_reply = "\u000AHELLO";
        if (strncmp(buffer, msg_hello.c_str(), msg_hello.size()) == 0) {
            std::string remote_ip = family == AF_INET ? get_ipv4(*client) : get_ipv6(*client);
            logger.info("Connection established (", remote_ip, ")");

            // send hello back
            if (SOCKET_ERROR ==
                sendto(s_in, msg_reply.c_str(), msg_reply.size(), 0, client, client_size)) {
                logger.error("Failed to reply hello");
                continue;
            }
            continue;
        }

        // -- transfer file --

        std::string hs_head = "\u000AHS";
        if (strncmp(buffer, hs_head.c_str(), hs_head.size()) != 0) continue;

        // receive handshake packet: [filesize, filename]
        logger.info("Received handshake packet");

        u_long file_size;
        memcpy(&file_size, buffer + hs_head.size(), sizeof(u_long));
        file_size = ntohl(file_size);
        std::string fn(buffer + hs_head.size() + sizeof(u_long));

        // check filename (starts with /, or contains .., or empty)
        if (fn.size() == 0 || fn[0] == '/' || fn.find("..") != std::string::npos) {
            logger.info("Refused to receive file: ", fn);
            // send reject
            std::string packet_reject = "\u000AREJECT";
            sendto(s_in, packet_reject.c_str(), packet_reject.size(), 0, client, client_size);
            continue;
        }

        // send ok
        std::string packet_ok = "\u000AOK";
        if (SOCKET_ERROR ==
            sendto(s_in, packet_ok.c_str(), packet_ok.size(), 0, client, client_size)) {
            logger.error("Failed to reply handshake");
            continue;
        }

        logger.info(ansi::bright_magenta, "Filesize: ", ansi::reset, fmt_size(file_size));
        logger.info(ansi::bright_magenta, "Filename: ", ansi::reset, fn);

        // create directory
        std::string save_directory = SAVE_PATH;
        int create_dir_err = CreateDirectory(save_directory.c_str(), NULL);
        int is_already_exists = create_dir_err == 0 && GetLastError() == ERROR_ALREADY_EXISTS;
        bool dir_err = create_dir_err == 0 && !is_already_exists;
        // create file
        std::ofstream file(save_directory + "/" + fn, std::ios::binary);
        if (dir_err || !file.is_open()) {
            logger.error("Failed to create file");
            // send drop
            std::string packet_drop = "\u000ADROP";
            sendto(s_in, packet_drop.c_str(), packet_drop.size(), 0, client, client_size);
            continue;
        }

        // receive file content
        u_long read_size = 0;
        u_long tot_chunk = file_size / BUFFER_SIZE + (file_size % BUFFER_SIZE != 0);
        u_long chunk = 0;
        u_long last_rate = 0;
        while (read_size < file_size) {
            ++chunk;

            // output
            u_long rate = chunk == 100 ? 100 : chunk * 100 / tot_chunk;
            if (chunk == 1 || chunk == tot_chunk || last_rate != rate) {
                std::string s = "";
                if (chunk > 1) s.append(ANSI_DELLINE);
                last_rate = rate;
                logger.print(s, logger.get_colored_prefix(Logger::Level::INFO), "Receiving file (",
                             rate, "%, ", chunk, "/", tot_chunk, ")");
            }

            // receive data
            bytes_received = recvfrom(s_in, buffer, BUFFER_SIZE, 0, client, &client_size);
            if (bytes_received == SOCKET_ERROR) {
                logger.error("Failed to receive data");
                file.close();
                continue;
            }

            // write file
            size_t write_size = std::min(file_size - read_size, (u_long)BUFFER_SIZE);
            file.write(buffer, write_size);
            read_size += write_size;

            // send receive status (chunk)
            std::string packet_received = "\u000ARECEIVED";
            u_long chunk_net = htonl(chunk);
            char* packet_rcv_buf = new char[packet_received.size() + sizeof(chunk_net)];
            memcpy(packet_rcv_buf, packet_received.c_str(), packet_received.size());
            memcpy(packet_rcv_buf + packet_received.size(), &chunk_net, sizeof(chunk_net));
            if (SOCKET_ERROR == sendto(s_in, packet_rcv_buf,
                                       packet_received.size() + sizeof(chunk_net), 0, client,
                                       client_size)) {
                logger.error("Failed to confirm received chunk");
                file.close();
                delete[] packet_rcv_buf;
                continue;
            }
            delete[] packet_rcv_buf;
        }

        logger.info(ansi::green, "File received", ansi::reset, " (", tot_chunk,
                    (tot_chunk > 1 ? " chunks" : " chunk"), " total)");

        file.close();

        // send receive status (done)
        packet_ok = "\u000ADONE";
        if (SOCKET_ERROR ==
            sendto(s_in, packet_ok.c_str(), packet_ok.size(), 0, client, client_size)) {
            logger.error("Failed to confirm file received");
            continue;
        }
    }

    closesocket(s_in);
}

int main(int argc, char* argv[]) {
    logger.set_color(Logger::Level::INFO, ansi::bright_cyan);

    if (argc != 2) {
        logger.print("Usage: udp_server <port>");
        return 0;
    }

    const int PORT = atoi(argv[1]);

    // create winsock
    WSAData wsData;
    int ws_ok = WSAStartup(MAKEWORD(2, 2), &wsData);
    if (ws_ok != 0) {
        logger.error("WSAStartup");
        return 0;
    }

    // create socket
    SOCKET s4 = create_socket(PORT, AF_INET);
    SOCKET s6 = create_socket(PORT, AF_INET6);

    if (s4 == INVALID_SOCKET && s6 == INVALID_SOCKET) {
        logger.error("Failed to create socket");
        return 0;
    }

    logger.info("Server is running at port ", PORT);

    bool ipv4_active = s4 != INVALID_SOCKET;
    bool ipv6_active = s6 != INVALID_SOCKET;
    std::thread threadIPv4(handle_socket, std::ref(s4), AF_INET);
    std::thread threadIPv6(handle_socket, std::ref(s6), AF_INET6);
    if (ipv4_active) threadIPv4.join();
    if (ipv6_active) threadIPv6.join();

    WSACleanup();

    return 0;
}
