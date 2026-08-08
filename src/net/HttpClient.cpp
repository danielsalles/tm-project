#include "net/HttpClient.h"

#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socklen_t = int;
#define CLOSESOCK closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSESOCK close
#endif

namespace tmx {

HttpUrl ParseHttpUrl(const char* url) {
    HttpUrl out;
    if (!url)
        return out;
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0)
        p += 7;
    else if (strstr(url, "://"))
        return out; // some other scheme — not supported

    // host[:port][/path]
    const char* slash = strchr(p, '/');
    const char* hostEnd = slash ? slash : p + strlen(p);
    const char* colon = nullptr;
    for (const char* c = p; c < hostEnd; ++c) {
        if (*c == ':')
            colon = c;
    }
    const size_t hostLen = (colon ? colon : hostEnd) - p;
    if (hostLen == 0 || hostLen >= sizeof out.host)
        return out;
    memcpy(out.host, p, hostLen);
    out.host[hostLen] = 0;

    if (colon) {
        out.port = atoi(colon + 1);
        if (out.port <= 0 || out.port > 65535)
            return out;
    }
    if (slash) {
        snprintf(out.path, sizeof out.path, "%s", slash);
    }
    out.ok = true;
    return out;
}

namespace {

struct WsaInit {
    bool ok = true;
    WsaInit() {
#ifdef _WIN32
        WSADATA wsa{};
        ok = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#endif
    }
    ~WsaInit() {
#ifdef _WIN32
        if (ok)
            WSACleanup();
#endif
    }
};

// connect() with timeout via non-blocking + select.
int ConnectWithTimeout(const addrinfo* ai, int timeoutMs) {
    int fd = (int)socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0)
        return -1;

#ifdef _WIN32
    u_long nonblock = 1;
    ioctlsocket(fd, FIONBIO, &nonblock);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif

    int r = connect(fd, ai->ai_addr, (socklen_t)ai->ai_addrlen);
#ifdef _WIN32
    const bool inProgress = (r < 0 && WSAGetLastError() == WSAEWOULDBLOCK);
#else
    const bool inProgress = (r < 0 && errno == EINPROGRESS);
#endif
    if (r < 0 && !inProgress) {
        CLOSESOCK(fd);
        return -1;
    }
    if (inProgress) {
        fd_set wfds;
        FD_ZERO(&wfds);
#ifdef _WIN32
        FD_SET((SOCKET)fd, &wfds);
#else
        FD_SET(fd, &wfds);
#endif
        timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
        r = select(fd + 1, nullptr, &wfds, nullptr, &tv);
        if (r <= 0) {
            CLOSESOCK(fd);
            return -1;
        }
        int err = 0;
        socklen_t len = sizeof err;
        getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
        if (err != 0) {
            CLOSESOCK(fd);
            return -1;
        }
    }

    // recv timeout for the read loop (requires a blocking socket — restore
    // after the non-blocking connect above).
#ifdef _WIN32
    u_long blocking = 0;
    ioctlsocket(fd, FIONBIO, &blocking);
#else
    fcntl(fd, F_SETFL, flags);
#endif
    timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    return fd;
}

} // namespace

int HttpGet(const char* url, char* outBuf, int outCap, int timeoutMs) {
    if (!outBuf || outCap <= 1)
        return 0;
    outBuf[0] = 0;
    const HttpUrl u = ParseHttpUrl(url);
    if (!u.ok)
        return 0;

    WsaInit wsa;
    if (!wsa.ok)
        return 0;

    char portStr[8];
    snprintf(portStr, sizeof portStr, "%d", u.port);
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    addrinfo* res = nullptr;
    if (getaddrinfo(u.host, portStr, &hints, &res) != 0 || !res)
        return 0;

    int fd = -1;
    for (addrinfo* ai = res; ai && fd < 0; ai = ai->ai_next)
        fd = ConnectWithTimeout(ai, timeoutMs);
    freeaddrinfo(res);
    if (fd < 0)
        return 0;

    char req[512];
    const int reqLen = snprintf(req, sizeof req,
                                "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n",
                                u.path, u.host);
    int sent = 0;
    while (sent < reqLen) {
        const int n = (int)send(fd, req + sent, reqLen - sent, 0);
        if (n <= 0) {
            CLOSESOCK(fd);
            return 0;
        }
        sent += n;
    }

    std::string raw;
    raw.reserve(2048);
    char chunk[2048];
    for (;;) {
        const int n = (int)recv(fd, chunk, sizeof chunk, 0);
        if (n <= 0)
            break; // EOF (HTTP/1.0 close-delimited) or timeout
        raw.append(chunk, (size_t)n);
        if ((int)raw.size() > 1024 * 1024)
            break; // sanity cap
    }
    CLOSESOCK(fd);

    // Status line must be 2xx.
    if (raw.size() < 12 || strncmp(raw.c_str(), "HTTP/", 5) != 0)
        return 0;
    const int status = atoi(raw.c_str() + 9);
    if (status < 200 || status >= 300)
        return 0;

    // Body starts after the header terminator.
    const size_t hdrEnd = raw.find("\r\n\r\n");
    if (hdrEnd == std::string::npos)
        return 0;
    const size_t bodyOff = hdrEnd + 4;
    size_t bodyLen = raw.size() - bodyOff;
    if (bodyLen > (size_t)(outCap - 1))
        bodyLen = (size_t)(outCap - 1);
    memcpy(outBuf, raw.data() + bodyOff, bodyLen);
    outBuf[bodyLen] = 0;
    return (int)bodyLen;
}

int BaseGetHttpRequest(const char* url, char* outBuf, int outCap) {
    return HttpGet(url, outBuf, outCap);
}

}
