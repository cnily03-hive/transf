#include <WS2tcpip.h>
#include <winsock2.h>
#include <ws2ipdef.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#define TITLE_INFO "\033[0;34m[INFO]\033[0m "
#define TITLE_ERROR "\033[0;31m[ERROR]\033[0m "

#include "utils.h"

#define ANSI_DELLINE "\033[F\033[K"
#define ANSI_CYAN_L "\033[0;96m"
#define ANSI_BLUE_D "\033[38;5;31m"
#define ANSI_GREEN_D "\033[38;5;22m"
#define ANSI_GRAY_L "\033[0;90m"
#define ANSI_RESET "\033[0m"

#pragma comment(lib, "ws2_32.lib")

#define CHUNK_SIZE 1024

#define SOCKET_SND_TIMEOUT 10000
#define SOCKET_RCV_TIMEOUT 5000

bool is_ipv6(const std::string& ip) {
    struct sockaddr_in6 sa;
    int result = inet_pton(AF_INET6, ip.c_str(), &(sa.sin6_addr));
    return result == 1;
}

int bind_ipv4(SOCKET& s, sockaddr_in* hint, const char* ip, const int port) {
    memset(hint, 0, sizeof(*hint));
    hint->sin_family = AF_INET;
    hint->sin_addr.s_addr = htonl(INADDR_ANY);
    hint->sin_port = htons(port);
    int pton_ok = inet_pton(AF_INET, ip, &hint->sin_addr);
    return pton_ok;
}

int bind_ipv6(SOCKET& s, sockaddr_in6* hint, const char* ip, const int port) {
    memset(hint, 0, sizeof(*hint));
    hint->sin6_family = AF_INET6;
    hint->sin6_addr = in6addr_any;
    hint->sin6_port = htons(port);
    int pton_ok = inet_pton(AF_INET6, ip, &hint->sin6_addr);
    return pton_ok;
}

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

int main(int argc, char* argv[]) {
    if (argc != 3) {
        logger::print("Usage: udp_client <ip> <port>");
        return 0;
    }

    const std::string ip_address = argv[1];
    const int PORT = atoi(argv[2]);
    const int SND_TIMEOUT = SOCKET_SND_TIMEOUT;
    const int RCV_TIMEOUT = SOCKET_RCV_TIMEOUT;

    // create winsock
    WSAData wsData;
    int ws_ok = WSAStartup(MAKEWORD(2, 2), &wsData);
    if (ws_ok != 0) {
        logger::error("WSAStartup");
        return 0;
    }

    // create socket (auto IPv4 or IPv6)
    const int FAMILY = is_ipv6(ip_address) ? AF_INET6 : AF_INET;
    SOCKET s_out = socket(FAMILY, SOCK_DGRAM, IPPROTO_UDP);
    if (s_out == INVALID_SOCKET) {
        int err = WSAGetLastError();
        logger::error("Failed to create socket (code: ", err, ")");
        return 0;
    }

    // set socket opt
    int opt_ok_1 =
        setsockopt(s_out, SOL_SOCKET, SO_SNDTIMEO, (const char*)&SND_TIMEOUT, sizeof(SND_TIMEOUT));
    int opt_ok_2 =
        setsockopt(s_out, SOL_SOCKET, SO_RCVTIMEO, (const char*)&RCV_TIMEOUT, sizeof(RCV_TIMEOUT));
    if (opt_ok_1 == SOCKET_ERROR || opt_ok_2 == SOCKET_ERROR) {
        logger::error("Failed to set socket options");
        return 0;
    }

    // bind ip and port
    sockaddr_in server4;
    sockaddr_in6 server6;

    int bind_ok = FAMILY == AF_INET ? bind_ipv4(s_out, &server4, ip_address.c_str(), PORT)
                                    : bind_ipv6(s_out, &server6, ip_address.c_str(), PORT);
    if (bind_ok != 1) {
        logger::error("Failed to bind ip and port");
        return 0;
    }
    sockaddr* server = FAMILY == AF_INET ? (sockaddr*)&server4 : (sockaddr*)&server6;
    int server_size = FAMILY == AF_INET ? sizeof(server4) : sizeof(server6);

    // send hello packet (confirm connection)
    std::string msg_hello = "\u000AHELLO";
    std::string msg_ret = "\u000AHELLO";
    char hello_rcv[16];
    if (SOCKET_ERROR ==
        sendto(s_out, msg_hello.c_str(), msg_hello.size(), 0, server, server_size)) {
        logger::error("Cannot connect to server");
        return 0;
    }
    if (SOCKET_ERROR == recvfrom(s_out, hello_rcv, 7, 0, server, &server_size)) {
        logger::error("Cannot connect to server");
        return 0;
    }
    if (strncmp(hello_rcv, msg_ret.c_str(), msg_ret.size()) != 0) {
        logger::error("Server responsed an unexpected message");
        return 0;
    }
    logger::info("Connection established");

    bool exit = false;
    logger::print(ANSI_CYAN_L, "Type a file path to send, or type '@exit' to exit", ANSI_RESET);
    while (!exit) {
        std::string input;
        logger::instant(ANSI_GRAY_L, "> ", ANSI_RESET);
        getline(std::cin, input);
        input = trim(input);

        if (input.size() > 0 && !input.starts_with("@")) {
            std::string fore_str = join_string(ANSI_GRAY_L, "> ", ANSI_RESET, input);

            std::string fp = input;
            std::string fn = extract_fn(fp);
            std::ifstream file(fp, std::ios::binary);

            // check if file exists
            if (!file.is_open()) {
                logger::error("File not found: ", ANSI_GRAY_L, fp, ANSI_RESET);
                continue;
            }

            file.seekg(0, std::ios::end);
            std::streamsize file_size = file.tellg();

            u_long file_size_net = htonl(file_size);  // convert to network byte order

            // integrate hs_head, filesize and filename
            std::string hs_head = "\u000AHS";
            u_long handshake_size = hs_head.size() + sizeof(file_size_net) + fn.size();
            char* handshake_buf = new char[handshake_size];
            memcpy(handshake_buf, hs_head.c_str(), hs_head.size());
            memcpy(handshake_buf + hs_head.size(), &file_size_net, sizeof(file_size_net));
            memcpy(handshake_buf + hs_head.size() + sizeof(file_size_net), fn.c_str(), fn.size());

            // send handshake packet: [filesize, filename]
            if (SOCKET_ERROR ==
                sendto(s_out, handshake_buf, handshake_size, 0, server, server_size)) {
                logger::error("Failed to send handshake");
                delete[] handshake_buf;
                file.close();
                continue;
            }
            delete[] handshake_buf;

            // receive handshake packet
            char rcv_buf[16];
            int bytes_received = recvfrom(s_out, rcv_buf, 16, 0, server, &server_size);
            if (SOCKET_ERROR == bytes_received) {
                logger::error("Failed when receiving handshake");
                file.close();
                continue;
            }

            // check if handshake is ok
            if (strncmp(rcv_buf, "\u000AREJECT", 7) == 0) {
                logger::error("The server refused to receive the file");
                file.close();
                continue;
            }
            if (strncmp(rcv_buf, "\u000AOK", 3) != 0) {
                logger::error("Unexpected handshake response");
                file.close();
                continue;
            }

            // send file content
            const int BUFFER_SIZE = CHUNK_SIZE;
            char buffer[BUFFER_SIZE];
            u_long chunk = 0, tot_chunk = file_size / BUFFER_SIZE + (file_size % BUFFER_SIZE != 0);
            u_long last_rate = 0;
            file.seekg(0, std::ios::beg);
            while (!file.eof()) {
                ++chunk;

                // output
                u_long rate = chunk == tot_chunk ? 100 : chunk * 100 / tot_chunk;
                if (chunk == 1 || chunk == tot_chunk || last_rate != rate) {
                    last_rate = rate;
                    logger::print(ANSI_DELLINE, fore_str, ANSI_BLUE_D, "   Sending file (", rate,
                                  "%)", ANSI_RESET);
                }

                // send file content
                file.read(buffer, BUFFER_SIZE);
                int read_size = file.gcount();
                if (SOCKET_ERROR == sendto(s_out, buffer, read_size, 0, server, server_size)) {
                    logger::error("Failed to send file content (", chunk, "/", tot_chunk, ")");
                    break;
                }

                // receive status (chunk)
                bytes_received = recvfrom(s_out, rcv_buf, 16, 0, server, &server_size);
                if (SOCKET_ERROR == bytes_received) {
                    logger::error("Failed when receiving status (", chunk, "/", tot_chunk, ")");
                    break;
                }

                std::string rcv_status_head = "\u000ARECEIVED";
                if (strncmp(rcv_buf, rcv_status_head.c_str(), rcv_status_head.size()) != 0) {
                    logger::error("Unexpected receive status (", chunk, "/", tot_chunk, ")");
                }

                u_long chunk_rcv = 0;
                memcpy(&chunk_rcv, rcv_buf + rcv_status_head.size(), sizeof(chunk_rcv));
                chunk_rcv = ntohl(chunk_rcv);
                if (chunk_rcv != chunk) {
                    logger::error("Chunk mismatch (", chunk, " expected, ", chunk_rcv,
                                  " received)");
                    break;
                }
            }

            file.close();

            // receive status (done)
            bytes_received = recvfrom(s_out, rcv_buf, 16, 0, server, &server_size);
            if (SOCKET_ERROR == bytes_received) {
                logger::error("Failed when receiving status (done)");
                continue;
            }

            if (strncmp(rcv_buf, "\u000ADONE", 5) != 0) {
                logger::error("Unexpected receive status (done)");
                continue;
            }

            logger::print(ANSI_DELLINE, fore_str, ANSI_GREEN_D, "   (Sent)", ANSI_RESET);

        } else if (input.size() > 0 && input.starts_with("@")) {
            if (input == "@exit" || input == "@quit" || input == "@q") {
                exit = true;
            } else if (input == "@about") {
                logger::print("UDP Client v1.0.0");
                logger::print("Copytight (c) 2024 by Cnily03");
                logger::print("Licensed under the MIT License");
            } else if (input == "@") {
                logger::info("Empty command");
            } else {
                logger::error("Unkown command: ", input.substr(1));
            }
        }
    }

    closesocket(s_out);
    WSACleanup();

    return 0;
}
