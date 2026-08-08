#include "test_framework.h"

#include "net/HttpClient.h"

#include <cstring>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace tmx;

TEST(http, url_parse_full) {
    HttpUrl u = ParseHttpUrl("http://example.com:8080/marks/guild1.bmp");
    EXPECT_TRUE(u.ok);
    EXPECT_EQ(strcmp(u.host, "example.com"), 0);
    EXPECT_EQ(u.port, 8080);
    EXPECT_EQ(strcmp(u.path, "/marks/guild1.bmp"), 0);
}

TEST(http, url_parse_defaults) {
    HttpUrl u = ParseHttpUrl("http://192.168.0.1/file.bin");
    EXPECT_TRUE(u.ok);
    EXPECT_EQ(u.port, 80);
    EXPECT_EQ(strcmp(u.path, "/file.bin"), 0);

    HttpUrl bare = ParseHttpUrl("example.com");
    EXPECT_TRUE(bare.ok);
    EXPECT_EQ(strcmp(bare.path, "/"), 0);
}

TEST(http, url_parse_rejects) {
    EXPECT_FALSE(ParseHttpUrl("https://example.com/x").ok); // no TLS
    EXPECT_FALSE(ParseHttpUrl("").ok);
    EXPECT_FALSE(ParseHttpUrl(nullptr).ok);
    EXPECT_FALSE(ParseHttpUrl("http:///path").ok);
}

// Loopback integration: local server thread answers one canned response.
TEST(http, get_loopback) {
    int listener = (int)socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_TRUE(listener >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // ephemeral
    EXPECT_EQ(bind(listener, (sockaddr*)&addr, sizeof addr), 0);
    EXPECT_EQ(listen(listener, 1), 0);

    socklen_t alen = sizeof addr;
    EXPECT_EQ(getsockname(listener, (sockaddr*)&addr, &alen), 0);
    const int port = ntohs(addr.sin_port);

    const char* body = "WYD-GUILD-MARK-BODY";
    std::thread server([&]() {
        int conn = (int)accept(listener, nullptr, nullptr);
        if (conn < 0)
            return;
        char req[1024];
        recv(conn, req, sizeof req, 0);
        char resp[512];
        const int n = snprintf(resp, sizeof resp,
                               "HTTP/1.0 200 OK\r\nContent-Length: %d\r\n\r\n%s",
                               (int)strlen(body), body);
        send(conn, resp, n, 0);
#ifdef _WIN32
        closesocket(conn);
#else
        close(conn);
#endif
    });

    char url[128];
    snprintf(url, sizeof url, "http://127.0.0.1:%d/mark.bmp", port);
    char buf[256];
    const int got = HttpGet(url, buf, sizeof buf, 2000);
    server.join();
#ifdef _WIN32
    closesocket(listener);
#else
    close(listener);
#endif

    EXPECT_TRUE(got > 0);
    EXPECT_EQ(strcmp(buf, body), 0);
}

TEST(http, get_rejects_404) {
    int listener = (int)socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_TRUE(listener >= 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    EXPECT_EQ(bind(listener, (sockaddr*)&addr, sizeof addr), 0);
    EXPECT_EQ(listen(listener, 1), 0);
    socklen_t alen = sizeof addr;
    getsockname(listener, (sockaddr*)&addr, &alen);
    const int port = ntohs(addr.sin_port);

    std::thread server([&]() {
        int conn = (int)accept(listener, nullptr, nullptr);
        if (conn < 0)
            return;
        char req[1024];
        recv(conn, req, sizeof req, 0);
        const char* resp = "HTTP/1.0 404 Not Found\r\n\r\n";
        send(conn, resp, (int)strlen(resp), 0);
#ifdef _WIN32
        closesocket(conn);
#else
        close(conn);
#endif
    });

    char url[128];
    snprintf(url, sizeof url, "http://127.0.0.1:%d/x", port);
    char buf[64];
    const int got = HttpGet(url, buf, sizeof buf, 2000);
    server.join();
#ifdef _WIN32
    closesocket(listener);
#else
    close(listener);
#endif
    EXPECT_EQ(got, 0);
}
