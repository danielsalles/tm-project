#include "test_framework.h"

#include "net/CPSock.h"
#include "platform/Platform.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>

#ifdef _WIN32
  #include <winsock2.h>
  #define closesocket closesocket
  using socket_t = SOCKET;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <sys/time.h>
  #define closesocket close
  using socket_t = int;
#endif

struct TestPacket : MSG_STANDARD {
    int value;
    char text[32];
};

static void EchoServer(uint16_t port, std::atomic<bool>& ready, std::atomic<bool>& echoed) {
    socket_t srv = socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_TRUE(srv != (socket_t)-1);

    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));

    timeval tv{5, 0};
    setsockopt(srv, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) != 0) {
        tmtest::Fail("server bind", __FILE__, __LINE__);
        closesocket(srv);
        return;
    }
    if (listen(srv, 1) != 0) {
        tmtest::Fail("server listen", __FILE__, __LINE__);
        closesocket(srv);
        return;
    }
    ready = true;

    socket_t cli = accept(srv, nullptr, nullptr);
    if (cli == (socket_t)-1) {
        closesocket(srv);
        return;
    }

    uint32_t initCode = 0;
    if (recv(cli, (char*)&initCode, 4, MSG_WAITALL) == 4 && initCode == TM_INIT_CODE) {
        char buf[4096];
        int n = recv(cli, buf, sizeof(buf), 0);
        if (n > 0) {
            send(cli, buf, n, 0);
            echoed = true;
        }
    }

    closesocket(cli);
    closesocket(srv);
}

TEST(cpsock, echo_roundtrip) {
    CPSock sock;
    EXPECT_TRUE(sock.WSAInitialize());

    std::atomic<bool> ready{false}, echoed{false};
    const uint16_t port = 48152;
    std::thread server(EchoServer, port, std::ref(ready), std::ref(echoed));

    for (int i = 0; i < 100 && !ready; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(ready);

    unsigned int s = sock.ConnectServer("127.0.0.1", port, 0);
    EXPECT_TRUE(s != 0);
    if (!s) {
        server.join();
        return;
    }

    TestPacket pkt{};
    pkt.Type = 0x364;   // MSG_STANDARDPARM-ish, valor qualquer
    pkt.ID = 42;
    pkt.value = 0x12345678;
    strcpy(pkt.text, "wyd echo test");

    EXPECT_TRUE(sock.SendOneMessage((char*)&pkt, sizeof(pkt)));

    char* got = nullptr;
    int errCode = 0, errType = 0;
    for (int i = 0; i < 200 && !got; i++) {
        sock.Poll();
        got = sock.ReadMessage(&errCode, &errType);
        if (!got)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(got != nullptr);
    EXPECT_EQ(errCode, 0);
    if (got) {
        auto* rp = reinterpret_cast<TestPacket*>(got);
        EXPECT_EQ(rp->Type, 0x364);
        EXPECT_EQ(rp->ID, 42);
        EXPECT_EQ(rp->value, 0x12345678);
        EXPECT_TRUE(strcmp(rp->text, "wyd echo test") == 0);
    }

    sock.CloseSocket();
    server.join();
    EXPECT_TRUE(echoed);
}
