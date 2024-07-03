#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#include <cstddef>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

typedef uint8_t ip_version;
typedef int ip_family;
typedef int sock_type;

typedef ADDRINFO addrhint;  // addrhint has its `ai_addr` to nullptr
typedef ADDRINFO addrcoll;  // addrcoll is address collection with `ai_addr` calculated

enum IPVersion : uint8_t { IPv4 = 1, IPv6 = 1 << 1 };
enum SockType : uint8_t {
    TYPE_UNKOWN = 0,
    TYPE_STREAM = SOCK_STREAM,
    TYPE_DGRAM = SOCK_DGRAM,
    TYPE_RAW = SOCK_RAW
};

//===--------------------------------------------------===//
// struct ConnectInfo
//===--------------------------------------------------===//

struct ConnectInfo {
    std::string ip;
    int port = 0;
    SockType type = TYPE_UNKOWN;

    ConnectInfo() = default;
    // copy constructor
    ConnectInfo(const ConnectInfo& c) noexcept;
    // move constructor
    ConnectInfo(ConnectInfo&& c) noexcept;
    // constructor from parameters
    ConnectInfo(const std::string ip, int port, SockType type) noexcept;
    // constructor from addrcoll
    ConnectInfo(const addrcoll* info) noexcept;

    int to_addrcoll(addrcoll** result) const;
    bool valid();
    bool operator==(const ConnectInfo& c);
    std::string to_string(bool include_type = false);
};

//===--------------------------------------------------===//
// struct ServiceInfo
//===--------------------------------------------------===//

struct ServiceInfo {
    std::string host;
    std::string service;
    SockType type = TYPE_UNKOWN;

    ServiceInfo() = default;
    // copy constructor
    ServiceInfo(const ServiceInfo& s) noexcept;
    // move constructor
    ServiceInfo(ServiceInfo&& s) noexcept;
    // constructor from parameters
    ServiceInfo(const std::string host, std::string service, SockType type) noexcept;
    // constructor from addrcoll
    ServiceInfo(const addrcoll* info) noexcept;

    bool valid();
    bool operator==(const ServiceInfo& s);
};

//===--------------------------------------------------===//
// class BasicSocket
//===--------------------------------------------------===//

typedef std::lock_guard<std::mutex> LockGuard;
typedef std::unique_lock<std::mutex> UniqueLock;

enum StreamStatus {
    CLOSED = 0,
    LISTENING = 1,
    CONNECTING = 1 << 1,
    CONNECTED = 1 << 2,
    DISCONNECTING = 1 << 3,
    DISCONNECTED = 1 << 4,
};

#define SOCKET_NOT_INIT 2
#define ADDR_NOT_INIT 3
#define SOCKET_NOT_PREPARED 5
#define METHOD_NOT_IMPLEMENTED 6
#define ALREADY_SERVING 11

class BasicSocket;
class SocketPeer;
class SocketClient;
class SocketServer;

typedef std::function<int(const SocketPeer&)> PeerCloseHandler;
typedef std::function<int(const SocketServer&)> ServerCloseHandler;
typedef std::function<int(const SocketPeer&, const BasicSocket&)> StreamHandler;
typedef std::function<int(const char*, const int, const SocketPeer&, const BasicSocket&)>
    MessageHandler;

class BasicSocket {
   protected:
    mutable SOCKET m_sockfd = INVALID_SOCKET;
    mutable bool m_prop_hasbound = false;
    addrcoll m_addrcoll;

   public:
    BasicSocket() = default;
    // copy constructor
    BasicSocket(const BasicSocket& r) noexcept;
    // move constructor
    BasicSocket(BasicSocket&& r) noexcept;
    // constructor from ip and port
    BasicSocket(ConnectInfo& conn) noexcept;
    BasicSocket(std::string ip, int port, SockType socktype) noexcept;
    // constructor from addrcoll
    BasicSocket(addrcoll* paddr) noexcept;

    virtual ~BasicSocket() = default;

    SOCKET socket() const;
    addrcoll addr_info() const;

    ConnectInfo conn_info() const;  // connection info
    ServiceInfo serv_info() const;  // service info

    int init_socket();
    int bind_address();
    virtual int close() const;
    virtual int destroy() const;
    static int terminate();

    virtual bool ensure_socket() const;
    virtual bool ensure_addr() const;
    virtual bool ensure() const;

    int send_to(const char* buf, int len, const addrcoll* paddr) const;
    int send_to(const std::string str, const addrcoll* paddr) const;
    int send_to(const char* buf, int len, const ConnectInfo& conn) const;
    int send_to(const std::string str, const ConnectInfo& conn) const;
    int send_to(const char* buf, int len, const BasicSocket& r) const;
    int send_to(const std::string str, const BasicSocket& r) const;

    int recv_from(char* buf, int maxlen, const addrcoll* paddr) const;
    int recv_from(std::string& str, int maxlen, const addrcoll* paddr) const;
    int recv_from(char* buf, int maxlen, const ConnectInfo& conn) const;
    int recv_from(std::string& str, int maxlen, const ConnectInfo& conn) const;
    int recv_from(char* buf, int maxlen, const BasicSocket& r) const;
    int recv_from(std::string& str, int maxlen, const BasicSocket& r) const;

   public:
    static SocketServer create_server(const BasicSocket& r) noexcept;
    static SocketServer create_server(BasicSocket&& r) noexcept;
    static SocketServer create_server(const BasicSocket& r, int max_clients) noexcept;
    static SocketServer create_server(BasicSocket&& r, int max_clients) noexcept;

    static SocketClient create_client(const BasicSocket& r) noexcept;
    static SocketClient create_client(BasicSocket&& r) noexcept;
};

//===--------------------------------------------------===//
// class PeerSocket
//===--------------------------------------------------===//

class SocketPeer : public BasicSocket {
   protected:
    BasicSocket* m_p_bsock_from;
    mutable std::list<const PeerCloseHandler> m_pCloseHandlers;

   public:
    SocketPeer() = default;
    SocketPeer(const SocketPeer& r) noexcept;
    SocketPeer(SocketPeer&& r) noexcept;

    // You have to specify the information of address where the data comes from

    SocketPeer(BasicSocket* p_bsocket_from, const BasicSocket& bs) noexcept;
    SocketPeer(BasicSocket* p_bsocket_from, BasicSocket&& bs) noexcept;
    SocketPeer(BasicSocket* p_bsocket_from, addrcoll* paddr_peer) noexcept;
    // Use a existing socket (i.e. a returned value of `accept` or `connect` for stream socket)
    SocketPeer(BasicSocket* p_bsocket_from, addrcoll* paddr_peer, SOCKET s) noexcept;
    // TODO: pointer unsafe? shall we write ~SocketPeer() ?

    bool ensure_addr() const override;

    int send(const char* buf, int len) const;
    int send(const std::string str) const;

    int recv(char* buf, int maxlen) const;
    int recv(std::string& str, int maxlen) const;

    int onclose(const PeerCloseHandler& pHandler) const;
    int close() const override;
};

//===--------------------------------------------------===//
// class SocketServer
//===--------------------------------------------------===//

class SocketClient : public BasicSocket {
   protected:
    addrcoll m_saddrcoll;

   public:
    SocketClient() = default;
    SocketClient(const SocketClient& r) noexcept;
    SocketClient(SocketClient&& r) noexcept;
    SocketClient(const BasicSocket& bs) noexcept;
    SocketClient(BasicSocket&& bs) noexcept;

    int send(const char* buf, int len) const;
    int send(const std::string str) const;

    int recv(char* buf, int maxlen) const;
    int recv(std::string& str, int maxlen) const;

    int connect() const;
    int reconnect(bool force = false);

    ~SocketClient();
};
//===--------------------------------------------------===//
// class SocketServer
//===--------------------------------------------------===//

#define HANDLE_NEXT 0
#define HANDLE_END -1
#define HANDLE_ERROR 1

class SocketServer : public BasicSocket {
   protected:
    std::map<u_long, SocketPeer&> m_clients;  // TODO: unused
    mutable std::list<const StreamHandler> m_pStreamHandlers;
    mutable std::list<const MessageHandler> m_pMessageHandlers;
    mutable std::list<const ServerCloseHandler> m_pCloseHandlers;
    int m_max_clients = SOMAXCONN;
    mutable bool m_serving = false;
    mutable std::vector<std::thread> m_threads;
    mutable std::mutex m_mutex;

   private:
    int stream_serve_thread(const SocketPeer& peer);
    int message_thread(char* buf, int len, const SocketPeer& peer);
    int message_stream_thread(int buf_size, const SocketPeer& peer);

   public:
    SocketServer() = default;
    SocketServer(const SocketServer& r) noexcept;
    SocketServer(SocketServer&& r) noexcept;
    SocketServer(const BasicSocket& bs) noexcept;
    SocketServer(BasicSocket&& bs) noexcept;
    SocketServer(const BasicSocket& bs, int max_clients) noexcept;
    SocketServer(BasicSocket&& bs, int max_clients) noexcept;

    virtual ~SocketServer();

    // For stream socket (by order of registration)
    int use(const StreamHandler& pHandler);
    // For stream socket
    int listen() const;

    // For datagram socket (by order of registration)
    int onmessage(const MessageHandler& pHandler);

    // If `buf_size` <= 0, serve only for stream socket
    // else serve for message receiving
    int serve(int buf_size = -1);

    // Emit when the socket is closed
    // each handler will be called synchronously (by its order of registration)
    int onclose(const ServerCloseHandler& pHandler) const;

    int close() const override;
    int wait() const;
};

//===--------------------------------------------------===//
// Functions
//===--------------------------------------------------===//

addrhint get_hints(ip_version ip_ver, SockType socktype);
addrhint tcp_hints(ip_version ip_ver);
addrhint udp_hints(ip_version ip_ver);
addrhint raw_hints(ip_version ip_ver);

// When ip is "", results list will have ip addresses on all network interfaces
// Note that it may not include ip address(es) on loopback interface
int ip_addrcoll(const std::string ip, const int port, addrhint* hints, addrcoll** result);

// When ip is NULL, results list will have loopback ip address(es)
int ip_addrcoll(const char* ip, const int port, addrhint* hints, addrcoll** result);

bool is_sock_bound(SOCKET s);
bool is_peer_bound(SOCKET s);

ADDRINFO copy_addrinfo(const ADDRINFO* src, bool copy_all = true);

// Bind a addrinfo to a socket
// The parameter `info` is the result of `GetAddrInfo`, it shall has `ai_addr` calculated
int bind_to_socket(SOCKET s, addrcoll* info);

ip_version get_ipversion(const std::string ip);

bool check_ip(const std::string ip);
bool check_port(const int port);
bool check_port(std::string port);

std::string join_ip_port(const std::string ip, const int port, bool keep_interface = false);

#endif  // __NETWORK_H__