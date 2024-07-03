#include "network.h"

inline ip_family to_addr_family(ip_version ip_ver) {
    return (ip_ver & IPv4) && (ip_ver & IPv6) ? AF_UNSPEC
           : (ip_ver & IPv4)                  ? AF_INET
           : (ip_ver & IPv6)                  ? AF_INET6
                                              : AF_UNSPEC;
}

inline SockType to_my_socktype(int type) {
    return (type == SOCK_RAW)      ? TYPE_RAW
           : (type == SOCK_STREAM) ? TYPE_STREAM
           : (type == SOCK_DGRAM)  ? TYPE_DGRAM
                                   : TYPE_UNKOWN;
}

inline sock_type to_addr_socktype(SockType type) {
    return (type == TYPE_RAW)      ? SOCK_RAW
           : (type == TYPE_STREAM) ? SOCK_STREAM
           : (type == TYPE_DGRAM)  ? SOCK_DGRAM
                                   : SOCK_RAW;
}

inline int to_addr_protocol(SockType type, ip_version ip_ver) {
    return (type == TYPE_STREAM)  ? IPPROTO_TCP
           : (type == TYPE_DGRAM) ? IPPROTO_UDP
           : (ip_ver & IPv4)      ? IPPROTO_IP
                                  : IPPROTO_IPV6;
}

//===--------------------------------------------------===//
// struct ConnectInfo
//===--------------------------------------------------===//

// copy constructor
ConnectInfo::ConnectInfo(const ConnectInfo& c) noexcept : ip(c.ip), port(c.port), type(c.type) {}
// move constructor
ConnectInfo::ConnectInfo(ConnectInfo&& c) noexcept
    : ip(std::move(c.ip)), port(std::move(c.port)), type(std::move(c.type)) {}
// constructor from parameters
ConnectInfo::ConnectInfo(const std::string ip, int port, SockType type) noexcept
    : ip(ip), port(port), type(type) {}

// constructor from addrcoll
ConnectInfo::ConnectInfo(const addrcoll* info) noexcept {
    type = to_my_socktype(info->ai_socktype);
    char _ip[INET6_ADDRSTRLEN];
    int _port;
    int family = info->ai_addr->sa_family;
    if (family != AF_INET && family != AF_INET6)
        family = (info->ai_addrlen == sizeof(sockaddr_in)) ? AF_INET : AF_INET6;

    if (family == AF_INET) {  // IPv4
        inet_ntop(family, &((struct sockaddr_in*)info->ai_addr)->sin_addr, _ip, sizeof(_ip));
        _port = ntohs(((struct sockaddr_in*)info->ai_addr)->sin_port);
    } else {  // IPv6
        inet_ntop(family, &((struct sockaddr_in6*)info->ai_addr)->sin6_addr, _ip, sizeof(_ip));
        _port = ntohs(((struct sockaddr_in6*)info->ai_addr)->sin6_port);
    }
    ip = _ip;
    port = _port;
}

int ConnectInfo::to_addrcoll(addrcoll** result) const {
    auto hints = get_hints(get_ipversion(ip), type);

    int err = ip_addrcoll(ip, port, &hints, result);
    return err;
}

bool ConnectInfo::valid() { return !ip.empty() && port > 0 && port < 65536 && type != TYPE_UNKOWN; }
bool ConnectInfo::operator==(const ConnectInfo& c) {
    return strcmp(ip.c_str(), c.ip.c_str()) == 0 && port == c.port && type == c.type;
}
std::string ConnectInfo::to_string(bool include_type) {
    if (!valid()) return "";
    std::string s = join_ip_port(ip, port, false);
    if (include_type) s += type == TYPE_STREAM ? "/tcp" : type == TYPE_DGRAM ? "/udp" : "";
    return s;
}

//===--------------------------------------------------===//
// struct ServiceInfo
//===--------------------------------------------------===//

// copy constructor
ServiceInfo::ServiceInfo(const ServiceInfo& s) noexcept
    : host(s.host), service(s.service), type(s.type) {}
// move constructor
ServiceInfo::ServiceInfo(ServiceInfo&& s) noexcept
    : host(std::move(s.host)), service(std::move(s.service)), type(std::move(s.type)) {}
// constructor from parameters
ServiceInfo::ServiceInfo(const std::string host, std::string service, SockType type) noexcept
    : host(host), service(service), type(type) {}

// constructor from addrcoll
ServiceInfo::ServiceInfo(const addrcoll* info) noexcept {
    type = to_my_socktype(info->ai_socktype);
    char _host[NI_MAXHOST], _serv[NI_MAXSERV];
    int err = getnameinfo(info->ai_addr, info->ai_addrlen,  // address
                          _host, sizeof(_host),             // host to store the result
                          _serv, sizeof(_serv),             // service to store the result
                          NI_NUMERICHOST | NI_NUMERICSERV);
    if (err == 0) {
        host = _host;
        service = _serv;
    }
}

bool ServiceInfo::valid() { return !host.empty() && !service.empty() && type != TYPE_UNKOWN; }
bool ServiceInfo::operator==(const ServiceInfo& s) {
    return strcmp(host.c_str(), s.host.c_str()) == 0 &&
           strcmp(service.c_str(), s.service.c_str()) == 0 && type == s.type;
}

//===--------------------------------------------------===//
// class BasicSocket
//===--------------------------------------------------===//

// constructors

BasicSocket::BasicSocket(const BasicSocket& r) noexcept  // copy constructor
    : m_sockfd(r.m_sockfd), m_addrcoll(r.m_addrcoll) {}
BasicSocket::BasicSocket(BasicSocket&& r) noexcept  // move constructor
    : m_sockfd(r.m_sockfd), m_addrcoll(r.m_addrcoll) {
    ::closesocket(r.m_sockfd);
}

BasicSocket::BasicSocket(ConnectInfo& conn) noexcept
    : BasicSocket(conn.ip, conn.port, conn.type) {}  // constructor from ConnectInfo

BasicSocket::BasicSocket(std::string ip, int port,
                         SockType socktype) noexcept  // constructor from ip and port
    : m_sockfd(INVALID_SOCKET), m_addrcoll() {
    ip_version ip_ver = get_ipversion(ip);
    if (ip_ver == 0) return;
    addrhint hints = get_hints(ip_ver, socktype);
    addrcoll* result = nullptr;
    int err = ip_addrcoll(ip.c_str(), port, &hints, &result);
    if (err != 0) {
        FreeAddrInfo(result);
        return;
    }
    m_addrcoll = *result;
    m_sockfd = ::socket(m_addrcoll.ai_family, m_addrcoll.ai_socktype, m_addrcoll.ai_protocol);
    FreeAddrInfo(result);
}

// Note that the `paddr` is copied, it can be freed safely
BasicSocket::BasicSocket(addrcoll* paddr) noexcept  // constructor from addrcoll
    : m_addrcoll(copy_addrinfo(paddr)) {
    m_sockfd = ::socket(m_addrcoll.ai_family, m_addrcoll.ai_socktype, m_addrcoll.ai_protocol);
}

// BasicSocket::~BasicSocket() {} // TODO: destructor, wtf, influenced derived classes

// member functions

SOCKET BasicSocket::socket() const { return m_sockfd; }
addrcoll BasicSocket::addr_info() const { return m_addrcoll; }

ConnectInfo BasicSocket::conn_info() const { return ConnectInfo(&m_addrcoll); }
ServiceInfo BasicSocket::serv_info() const { return ServiceInfo(&m_addrcoll); }

bool BasicSocket::ensure_socket() const { return m_sockfd != INVALID_SOCKET; }
bool BasicSocket::ensure_addr() const { return m_addrcoll.ai_addr != nullptr; }
bool BasicSocket::ensure() const { return ensure_socket() && ensure_addr() && m_prop_hasbound; }

int BasicSocket::init_socket() {
    m_sockfd = ::socket(m_addrcoll.ai_family, m_addrcoll.ai_socktype, m_addrcoll.ai_protocol);
    return m_sockfd == INVALID_SOCKET ? 1 : 0;
}

int BasicSocket::bind_address() {
    if (!ensure_socket()) return SOCKET_NOT_INIT;
    if (!ensure_addr()) return ADDR_NOT_INIT;
    int err = bind_to_socket(m_sockfd, &m_addrcoll);
    if (err == 0) m_prop_hasbound = true;
    return err;
}

int BasicSocket::close() const {
    int err = ::closesocket(m_sockfd);
    m_prop_hasbound = is_sock_bound(m_sockfd);
    // TODO: add status (CONNECTING, CLOSING, CLOSED, etc.)
    return err;
}

int BasicSocket::destroy() const {
    int err = ::closesocket(m_sockfd);
    m_sockfd = INVALID_SOCKET;
    return err;
}

// Cleanup WSA is a breaking action, it will make all sockets invalid
int BasicSocket::terminate() { return WSACleanup(); }

// The following functions handle the data transportation between sockets
// We have to ensure the socket is valid before transporting
// While our local socket not prepared, the transportation will do nothing

// send

int BasicSocket::send_to(const char* buf, int len, const addrcoll* paddr) const {
    if (!ensure()) return SOCKET_NOT_PREPARED;
    int err = ::sendto(m_sockfd, buf, len, 0, paddr->ai_addr, (socklen_t)paddr->ai_addrlen);
    return err;
}
int BasicSocket::send_to(const std::string str, const addrcoll* paddr) const {
    if (!ensure()) return SOCKET_NOT_PREPARED;
    return send_to(str.c_str(), str.size(), paddr);
}
int BasicSocket::send_to(const char* buf, int len, const ConnectInfo& conn) const {
    if (!ensure()) return SOCKET_NOT_PREPARED;
    addrcoll* result = nullptr;
    int err = conn.to_addrcoll(&result);
    if (err != 0) return FreeAddrInfo(result), err;
    err = ::sendto(m_sockfd, buf, len, 0, result->ai_addr, result->ai_addrlen);
    return FreeAddrInfo(result), err;
}
int BasicSocket::send_to(const std::string str, const ConnectInfo& conn) const {
    if (!ensure()) return SOCKET_NOT_PREPARED;
    return send_to(str.c_str(), str.size(), conn);
}
int BasicSocket::send_to(const char* buf, int len, const BasicSocket& r) const {
    if (!ensure()) return SOCKET_NOT_PREPARED;
    int err = ::sendto(m_sockfd, buf, len, 0, r.m_addrcoll.ai_addr, r.m_addrcoll.ai_addrlen);
    return err;
}
int BasicSocket::send_to(const std::string str, const BasicSocket& r) const {
    if (!ensure()) return SOCKET_NOT_PREPARED;
    return send_to(str.c_str(), str.size(), r);
}

// recv

int BasicSocket::recv_from(char* buf, int maxlen, const addrcoll* paddr) const {
    if (!ensure()) return -1;
    int size = ::recvfrom(m_sockfd, buf, maxlen, 0, paddr->ai_addr, (socklen_t*)&paddr->ai_addrlen);
    return size;
}
int BasicSocket::recv_from(std::string& str, int maxlen, const addrcoll* paddr) const {
    if (!ensure()) return -1;
    char* _buf = new char[maxlen];
    int size = recv_from(_buf, maxlen, paddr);
    str = std::string(_buf);
    delete[] _buf;
    return size;
}
int BasicSocket::recv_from(char* buf, int maxlen, const ConnectInfo& conn) const {
    if (!ensure()) return -1;
    addrcoll* result = nullptr;
    int err = conn.to_addrcoll(&result);
    if (err != 0) return FreeAddrInfo(result), -1;
    int size =
        ::recvfrom(m_sockfd, buf, maxlen, 0, result->ai_addr, (socklen_t*)&result->ai_addrlen);
    return FreeAddrInfo(result), size;
}
int BasicSocket::recv_from(std::string& str, int maxlen, const ConnectInfo& conn) const {
    if (!ensure()) return -1;
    char* _buf = new char[maxlen];
    int size = recv_from(_buf, maxlen, conn);
    str.clear();
    if (size >= 0) str.append(_buf, size);
    delete[] _buf;
    return size;
}
int BasicSocket::recv_from(char* buf, int maxlen, const BasicSocket& r) const {
    if (!ensure()) return -1;
    int size = ::recvfrom(m_sockfd, buf, maxlen, 0, r.m_addrcoll.ai_addr,
                          (socklen_t*)&r.m_addrcoll.ai_addrlen);
    return size;
}
int BasicSocket::recv_from(std::string& str, int maxlen, const BasicSocket& r) const {
    if (!ensure()) return -1;
    char* _buf = new char[maxlen];
    int size = recv_from(_buf, maxlen, r);
    str.clear();
    if (size >= 0) str.append(_buf, size);
    delete[] _buf;
    return size;
}

// static functions

SocketServer BasicSocket::create_server(const BasicSocket& r) noexcept { return SocketServer(r); }
SocketServer BasicSocket::create_server(BasicSocket&& r) noexcept {
    return SocketServer(std::move(r));
}
SocketServer BasicSocket::create_server(const BasicSocket& r, int max_clients) noexcept {
    return SocketServer(r, max_clients);
}
SocketServer BasicSocket::create_server(BasicSocket&& r, int max_clients) noexcept {
    return SocketServer(std::move(r), max_clients);
}

SocketClient BasicSocket::create_client(const BasicSocket& r) noexcept { return SocketClient(r); }
SocketClient BasicSocket::create_client(BasicSocket&& r) noexcept {
    return SocketClient(std::move(r));
}

//===--------------------------------------------------===//
// SocketPeer
//===--------------------------------------------------===//

SocketPeer::SocketPeer(const SocketPeer& r) noexcept
    : BasicSocket(r), m_p_bsock_from(r.m_p_bsock_from) {}

SocketPeer::SocketPeer(SocketPeer&& r) noexcept
    : BasicSocket(std::move(r)), m_p_bsock_from(r.m_p_bsock_from) {}

SocketPeer::SocketPeer(BasicSocket* p_bsocket_from, const BasicSocket& bs) noexcept
    : BasicSocket(bs), m_p_bsock_from(p_bsocket_from) {}

SocketPeer::SocketPeer(BasicSocket* p_bsocket_from, BasicSocket&& bs) noexcept
    : BasicSocket(std::move(bs)), m_p_bsock_from(p_bsocket_from) {}

SocketPeer::SocketPeer(BasicSocket* p_bsocket_from, addrcoll* paddr_peer) noexcept
    : BasicSocket(paddr_peer), m_p_bsock_from(p_bsocket_from) {}

SocketPeer::SocketPeer(BasicSocket* p_bsocket_from, addrcoll* paddr_peer, SOCKET s_peer) noexcept
    : BasicSocket(paddr_peer), m_p_bsock_from(p_bsocket_from) {
    m_sockfd = s_peer;
    m_prop_hasbound = is_peer_bound(s_peer);
}

bool SocketPeer::ensure_addr() const {
    return ::BasicSocket::ensure_socket() && m_p_bsock_from->ensure_addr();
}

int SocketPeer::send(const char* buf, int totlen) const {
    bool is_both_stream = m_addrcoll.ai_socktype == SOCK_STREAM &&
                          m_p_bsock_from->addr_info().ai_socktype == SOCK_STREAM;
    if (is_both_stream) {
        if (!ensure_socket() || !ensure_addr()) return SOCKET_NOT_PREPARED;
        int err = ::send(m_sockfd, buf, totlen, 0);
        return err;
    } else {
        if (!ensure_addr()) return ADDR_NOT_INIT;
        int err = ::sendto(m_p_bsock_from->socket(), buf, totlen, 0, m_addrcoll.ai_addr,
                           (socklen_t)m_addrcoll.ai_addrlen);
        return err;
    }
}

int SocketPeer::send(const std::string str) const { return send(str.c_str(), str.size()); }

int SocketPeer::recv(char* buf, int maxlen) const {
    bool is_both_stream = m_addrcoll.ai_socktype == SOCK_STREAM &&
                          m_p_bsock_from->addr_info().ai_socktype == SOCK_STREAM;
    if (is_both_stream) {
        if (!ensure_socket() || !ensure_addr()) return SOCKET_NOT_PREPARED;
        int size = ::recv(m_sockfd, buf, maxlen, 0);
        return size;
    } else {
        if (!ensure_addr()) return ADDR_NOT_INIT;
        int size = ::recvfrom(m_p_bsock_from->socket(), buf, maxlen, 0, m_addrcoll.ai_addr,
                              (socklen_t*)&m_addrcoll.ai_addrlen);
        return size;
    }
}

int SocketPeer::recv(std::string& str, int maxlen) const {
    char* _buf = new char[maxlen];
    int size = recv(_buf, maxlen);
    str.clear();
    if (size >= 0) str.append(_buf);
    delete[] _buf;
    return size;
}

int SocketPeer::onclose(const PeerCloseHandler& pHandler) const {
    // if (pHandler == nullptr) return 1;
    m_pCloseHandlers.push_back(pHandler);
    return 0;
}

int SocketPeer::close() const {
    if (!ensure()) return 0;
    for (auto pHandler : m_pCloseHandlers) {
        pHandler(*this);
    }
    int err = ::closesocket(m_sockfd);
    m_prop_hasbound = is_peer_bound(m_sockfd);
    return err;
}

//===--------------------------------------------------===//
// SocketPeer
//===--------------------------------------------------===//

SocketClient::SocketClient(const SocketClient& r) noexcept
    : BasicSocket(r), m_saddrcoll(r.m_saddrcoll) {}
SocketClient::SocketClient(SocketClient&& r) noexcept
    : BasicSocket(std::move(r)), m_saddrcoll(r.m_saddrcoll) {}
SocketClient::SocketClient(const BasicSocket& bs) noexcept : BasicSocket(bs) {
    m_saddrcoll = copy_addrinfo(&m_addrcoll, false);
}
SocketClient::SocketClient(BasicSocket&& bs) noexcept : BasicSocket(std::move(bs)) {
    m_saddrcoll = copy_addrinfo(&m_addrcoll, false);
}

SocketClient::~SocketClient() {
    destroy();
    delete &m_saddrcoll;  // TODO: or FreeAddrInfo ?
}

int SocketClient::send(const char* buf, int totlen) const {
    bool is_both_stream =
        m_addrcoll.ai_socktype == SOCK_STREAM && m_saddrcoll.ai_socktype == SOCK_STREAM;
    if (is_both_stream) {
        if (!ensure_addr()) return ADDR_NOT_INIT;
        int err = ::send(m_sockfd, buf, totlen, 0);
        return err;
    } else {
        if (!ensure_addr()) return ADDR_NOT_INIT;
        int err = ::sendto(m_sockfd, buf, totlen, 0, m_saddrcoll.ai_addr,
                           (socklen_t)m_saddrcoll.ai_addrlen);
        return err;
    }
}

int SocketClient::send(const std::string str) const { return send(str.c_str(), str.size()); }
int SocketClient::recv(char* buf, int maxlen) const {
    bool is_both_stream =
        m_addrcoll.ai_socktype == SOCK_STREAM && m_saddrcoll.ai_socktype == SOCK_STREAM;
    if (is_both_stream) {
        if (!ensure_addr()) return SOCKET_NOT_PREPARED;
        int size = ::recv(m_sockfd, buf, maxlen, 0);
        return size;
    } else {
        if (!ensure_addr()) return ADDR_NOT_INIT;
        int size = ::recvfrom(m_sockfd, buf, maxlen, 0, m_saddrcoll.ai_addr,
                              (socklen_t*)&m_saddrcoll.ai_addrlen);
        return size;
    }
}

int SocketClient::connect() const {
    if (!ensure_socket()) return SOCKET_NOT_PREPARED;
    auto conn = conn_info();
    if (!conn.valid()) return SOCKET_NOT_PREPARED;

    if (conn.type == TYPE_STREAM) {
        int err = ::connect(m_sockfd, m_saddrcoll.ai_addr, m_saddrcoll.ai_addrlen);
        if (err != 0) closesocket(m_sockfd);
        return err;
    } else if (conn.type == TYPE_DGRAM) {
        return METHOD_NOT_IMPLEMENTED;
    } else {
        return METHOD_NOT_IMPLEMENTED;
    }
}

int SocketClient::reconnect(bool force) {
    if (force) close();
    init_socket();
    return connect();
}

int SocketClient::recv(std::string& str, int maxlen) const {
    char* _buf = new char[maxlen];
    int size = recv(_buf, maxlen);
    str.clear();
    if (size >= 0) str.append(_buf);
    delete[] _buf;
    return size;
}

//===--------------------------------------------------===//
// SocketServer
//===--------------------------------------------------===//

SocketServer::SocketServer(const SocketServer& r) noexcept
    : BasicSocket(r),
      m_clients(r.m_clients),
      m_pStreamHandlers(r.m_pStreamHandlers),
      m_max_clients(r.m_max_clients) {}
SocketServer::SocketServer(SocketServer&& r) noexcept
    : BasicSocket(std::move(r)),
      m_clients(std::move(r.m_clients)),
      m_pStreamHandlers(std::move(r.m_pStreamHandlers)),
      m_max_clients(std::move(r.m_max_clients)) {}
SocketServer::SocketServer(const BasicSocket& bs) noexcept : BasicSocket(bs) {}
SocketServer::SocketServer(BasicSocket&& bs) noexcept : BasicSocket(std::move(bs)) {}
SocketServer::SocketServer(const BasicSocket& bs, int max_clients) noexcept
    : BasicSocket(bs), m_max_clients(max_clients) {}
SocketServer::SocketServer(BasicSocket&& r, int max_clients) noexcept
    : BasicSocket(std::move(r)), m_max_clients(max_clients) {}

SocketServer::~SocketServer() {
    close();
    wait();
}

int SocketServer::use(const StreamHandler& pHandler) {
    // if (pHandler == nullptr) return 1;
    m_pStreamHandlers.push_back(pHandler);
    return 0;
}

int SocketServer::listen() const {
    if (!ensure_socket()) return SOCKET_NOT_PREPARED;
    auto conn = conn_info();
    if (!conn.valid()) return SOCKET_NOT_PREPARED;

    if (conn.type == TYPE_STREAM) {
        int err = ::listen(m_sockfd, m_max_clients);
        if (err != 0) closesocket(m_sockfd);
        return err;
    } else if (conn.type == TYPE_DGRAM) {
        return METHOD_NOT_IMPLEMENTED;
    } else {
        return METHOD_NOT_IMPLEMENTED;
    }
}

int SocketServer::stream_serve_thread(const SocketPeer& peer) {
    int err = 0;
    for (auto pHandler : m_pStreamHandlers) {
        int err = pHandler(peer, *(BasicSocket*)this);
        if (err == HANDLE_NEXT) continue;
        if (err == HANDLE_END) break;
        // TODO: Shall we just break if error occurs? Or to use promise?
        // Note that this TODO influences:
        // - stream_serve_thread
        // - message_thread
        if (err == HANDLE_ERROR) break;
        // default break
        break;
    }
    return err;
};

int SocketServer::message_thread(char* buf, int len, const SocketPeer& peer) {
    int err = 0;
    for (auto pHandler : m_pMessageHandlers) {
        int err = pHandler(buf, len, peer, *(BasicSocket*)this);
        if (err == HANDLE_NEXT) continue;
        if (err == HANDLE_END) break;
        if (err == HANDLE_ERROR) break;
        break;
    }
    delete[] buf;
    return err;
}

int SocketServer::message_stream_thread(int buf_size, const SocketPeer& peer) {
    int err = 0;

    char* buf_cache = new char[buf_size];
    // ZeroMemory(buf_cache, buf_size);

    bool is_alive = true;
    while (is_alive) {
        int size = peer.recv(buf_cache, buf_size);
        if (size <= 0) {
            is_alive = false;
            continue;
        }

        UniqueLock lock(m_mutex);
        if (!m_serving) {
            is_alive = false;
            continue;
        };
        char* buf = new char[size];
        memcpy(buf, buf_cache, size);
        err = message_thread(buf, size, peer);
    }
    peer.close();
    return err;
}

int SocketServer::serve(int buf_size) {
    {
        UniqueLock lock(m_mutex);
        if (m_serving) return ALREADY_SERVING;
        m_serving = true;
    }

    if (!ensure_socket()) return SOCKET_NOT_PREPARED;

#define noloop_continue \
    {                   \
        loop = false;   \
        continue;       \
    }

    if (m_addrcoll.ai_socktype == SOCK_STREAM) {
        // serve for stream socket
        bool loop = true;
        while (loop) {
            UniqueLock lock(m_mutex);
            if (!m_serving) noloop_continue;
            lock.unlock();

            SOCKET client_s = ::accept(socket(), nullptr, nullptr);
            if (client_s == INVALID_SOCKET) continue;

            addrcoll c_addrcoll = copy_addrinfo(&m_addrcoll);
            sockaddr_storage addr_st;
            int adr_st_len = sizeof(addr_st);
            int err = getpeername(client_s, (sockaddr*)&addr_st, &adr_st_len);
            if (err != 0) continue;
            c_addrcoll.ai_addr = (sockaddr*)&addr_st;
            c_addrcoll.ai_addrlen = adr_st_len;
            SocketPeer peer(this, &c_addrcoll, client_s);
            if (!peer.ensure()) continue;

            if (buf_size > 0) {
                // message receiving
                lock.lock();
                if (!m_serving) noloop_continue;
                lock.unlock();

                m_threads.emplace_back(&SocketServer::message_stream_thread, this, buf_size,
                                       std::ref(peer));
            } else {
                // new thread for each client
                lock.lock();
                if (!m_serving) noloop_continue;
                m_threads.emplace_back(&SocketServer::stream_serve_thread, this, std::ref(peer));
            }
            // unlock automatically
        }
    } else if (buf_size > 0) {
        // serve not for stream socket
        // message receiving

        char* buf_cache = new char[buf_size];
        // ZeroMemory(buf_cache, buf_size);

        bool loop = true;
        sockaddr_storage* addr_cache = new sockaddr_storage;
        while (loop) {
            UniqueLock lock(m_mutex);
            if (!m_serving) noloop_continue;
            lock.unlock();

            // receive
            int addrlen = m_addrcoll.ai_addrlen;
            ZeroMemory(addr_cache, sizeof(sockaddr_storage));
            int size =
                ::recvfrom(m_sockfd, buf_cache, buf_size, 0, (sockaddr*)addr_cache, &addrlen);
            if (size <= 0) continue;
            // get peer addrcoll
            addrcoll c_addrcoll = copy_addrinfo(&m_addrcoll, false);
            c_addrcoll.ai_addr = (sockaddr*)addr_cache;
            c_addrcoll.ai_addrlen = addrlen;
            // create peer
            SocketPeer peer(this, &c_addrcoll);
            if (!peer.ensure_addr()) continue;

            lock.lock();
            if (!m_serving) noloop_continue;
            char* buf = new char[size];
            memcpy(buf, buf_cache, size);
            m_threads.emplace_back(&SocketServer::message_thread, this, buf, size, peer);
            // unlock automatically
        }

        delete[] buf_cache;
    }

#undef noloop_continue

    {
        UniqueLock lock(m_mutex);
        if (m_serving) m_serving = false;
    }

    return 0;
}

int SocketServer::onmessage(const MessageHandler& pHandler) {
    // if (pHandler == nullptr) return 1;
    m_pMessageHandlers.push_back(pHandler);
    return 0;
}

int SocketServer::onclose(const ServerCloseHandler& pHandler) const {
    // if (pHandler == nullptr) return 1;
    m_pCloseHandlers.push_back(pHandler);
    return 0;
}

int SocketServer::close() const {
    UniqueLock lock(m_mutex);

    if (!m_serving) return 0;  // not serving, no need to close
    lock.unlock();

    // closing
    // emit all close handlers
    for (auto pHandler : m_pCloseHandlers) {
        pHandler(*this);
    }

    // cancel serving
    m_serving = false;  // TODO: use StreamStatus
    // close the socket
    return ::BasicSocket::close();
}

int SocketServer::wait() const {
    for (auto& t : m_threads) {
        if (t.joinable()) t.join();
    }
    return 0;
}

//===--------------------------------------------------===//
// Functions
//===--------------------------------------------------===//

addrhint get_hints(ip_version ip_ver, SockType socktype) {
    addrhint hints;
    ZeroMemory(&hints, sizeof(addrhint));
    if (!ip_ver) return hints;
    hints.ai_family = to_addr_family(ip_ver);
    hints.ai_socktype = to_addr_socktype(socktype);
    hints.ai_protocol = to_addr_protocol(socktype, ip_ver);
    hints.ai_flags = AI_PASSIVE;
    return hints;
}

addrhint tcp_hints(ip_version ip_ver) { return get_hints(ip_ver, TYPE_STREAM); }
addrhint udp_hints(ip_version ip_ver) { return get_hints(ip_ver, TYPE_DGRAM); }
addrhint raw_hints(ip_version ip_ver) { return get_hints(ip_ver, TYPE_RAW); }

// When ip is "", results list will have ip addresses on all network interfaces
// Note that it may not include ip address(es) on loopback interface
int ip_addrcoll(const std::string ip, const int port, addrhint* hint, addrcoll** result) {
    int err = GetAddrInfo(ip.c_str(), std::to_string(port).c_str(), hint, result);
    return err;
}

// When ip is NULL, results list will have loopback ip address(es)
int ip_addrcoll(const char* ip, const int port, addrhint* hint, addrcoll** result) {
    int err = GetAddrInfo(ip, std::to_string(port).c_str(), hint, result);
    return err;
}

ADDRINFO copy_addrinfo(const ADDRINFO* src, bool copy_all) {
    ADDRINFO dest;
    ADDRINFO* pcur = &dest;

    for (const ADDRINFO* paddr = src; paddr != nullptr; paddr = paddr->ai_next) {
        memcpy(pcur, paddr, sizeof(ADDRINFO));
        pcur->ai_addr = nullptr;
        pcur->ai_canonname = nullptr;
        pcur->ai_next = nullptr;

        // copy ai_addr
        if (paddr->ai_addr) {
            pcur->ai_addr = (struct sockaddr*)malloc(paddr->ai_addrlen);
            memcpy(pcur->ai_addr, paddr->ai_addr, paddr->ai_addrlen);
        }

        // copy ai_canonname
        if (paddr->ai_canonname) {
            pcur->ai_canonname = _strdup(paddr->ai_canonname);
        }

        if (!copy_all) break;

        // copy ai_next (prepare)
        if (paddr->ai_next) {
            pcur->ai_next = (ADDRINFO*)malloc(sizeof(ADDRINFO));
            pcur = pcur->ai_next;
        }
    }

    // return std::move(dest);
    return dest;
}

// Bind a addrinfo to a socket
// The parameter `info` is the result of `GetAddrInfo`, it shall has `ai_addr` calculated
int bind_to_socket(SOCKET s, addrcoll* info) {
    int err = bind(s, info->ai_addr, (socklen_t)info->ai_addrlen);
    return err;
}

bool is_sock_bound(SOCKET s) {
    sockaddr_storage addr;
    int len = sizeof(addr);
    int err = getsockname(s, (sockaddr*)&addr, &len);
    if (err != 0) return false;
    if (addr.ss_family == AF_INET) {
        return ((sockaddr_in*)&addr)->sin_port != 0;
    } else if (addr.ss_family == AF_INET6) {
        return ((sockaddr_in6*)&addr)->sin6_port != 0;
    }
    return false;
}

bool is_peer_bound(SOCKET s) {
    sockaddr_storage addr;
    int len = sizeof(addr);
    int err = getpeername(s, (sockaddr*)&addr, &len);
    if (err != 0) return false;
    if (addr.ss_family == AF_INET) {
        return ((sockaddr_in*)&addr)->sin_port != 0;
    } else if (addr.ss_family == AF_INET6) {
        return ((sockaddr_in6*)&addr)->sin6_port != 0;
    }
    return false;
}

ip_version get_ipversion(const std::string ip) {
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr));
    if (result == 1) return IPv4;

    struct sockaddr_in6 sa6;
    result = inet_pton(AF_INET6, ip.c_str(), &(sa6.sin6_addr));
    if (result == 1) return IPv6;

    return 0;
}

bool check_ip(const std::string ip) { return get_ipversion(ip) != 0; }
bool check_port(const int port) { return port > 0 && port < 65536; }
bool check_port(std::string port) {
    try {
        return check_port(std::stoi(port));
    } catch (...) {
        return false;
    }
}

std::string join_ip_port(const std::string ip, const int port, bool keep_interface) {
    ip_version ip_ver = get_ipversion(ip);
    if (ip_ver == 0) return "";
    if (!check_port(port)) return "";
    if (ip_ver & IPv6) {
        // substr is used to remove the suffix like `%eth0`
        std::string final_ip = keep_interface ? ip : ip.substr(0, ip.find('%'));
        std::string s = "[" + final_ip + "]:" + std::to_string(port);
        return s;
    } else {
        std::string s = ip + ":" + std::to_string(port);
        return s;
    }
}