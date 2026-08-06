#include "net/CPSock.h"

#include "platform/Platform.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>

  #define closesocket close
  #define SOCKET_ERROR (-1)
  using SOCKET = int;
  static int WSAGetLastError() { return errno; }
#endif

int ConnectPort = 0;

unsigned char pKeyWord[512] = {
    0x84, 0x87, 0x37, 0xd7, 0xea, 0x79, 0x91, 0x7d, 0x4b, 0x4b, 0x85, 0x7d, 0x87, 0x81, 0x91, 0x7c, 0x0f, 0x73, 0x91, 0x91, 0x87, 0x7d, 0x0d, 0x7d, 0x86, 0x8f, 0x73, 0x0f, 0xe1, 0xdd, 0x85, 0x7d,
    0x05, 0x7d, 0x85, 0x83, 0x87, 0x9c, 0x85, 0x33, 0x0d, 0xe2, 0x87, 0x19, 0x0f, 0x79, 0x85, 0x86, 0x37, 0x7d, 0xd7, 0xdd, 0xe9, 0x7d, 0xd7, 0x7d, 0x85, 0x79, 0x05, 0x7d, 0x0f, 0xe1, 0x87, 0x7e,
    0x23, 0x87, 0xf5, 0x79, 0x5f, 0xe3, 0x4b, 0x83, 0xa3, 0xa2, 0xae, 0x0e, 0x14, 0x7d, 0xde, 0x7e, 0x85, 0x7a, 0x85, 0xaf, 0xcd, 0x7d, 0x87, 0xa5, 0x87, 0x7d, 0xe1, 0x7d, 0x88, 0x7d, 0x15, 0x91,
    0x23, 0x7d, 0x87, 0x7c, 0x0d, 0x7a, 0x85, 0x87, 0x17, 0x7c, 0x85, 0x7d, 0xac, 0x80, 0xbb, 0x79, 0x84, 0x9b, 0x5b, 0xa5, 0xd7, 0x8f, 0x05, 0x0f, 0x85, 0x7e, 0x85, 0x80, 0x85, 0x98, 0xf5, 0x9d,
    0xa3, 0x1a, 0x0d, 0x19, 0x87, 0x7c, 0x85, 0x7d, 0x84, 0x7d, 0x85, 0x7e, 0xe7, 0x97, 0x0d, 0x0f, 0x85, 0x7b, 0xea, 0x7d, 0xad, 0x80, 0xad, 0x7d, 0xb7, 0xaf, 0x0d, 0x7d, 0xe9, 0x3d, 0x85, 0x7d,
    0x87, 0xb7, 0x23, 0x7d, 0xe7, 0xb7, 0xa3, 0x0c, 0x87, 0x7e, 0x85, 0xa5, 0x7d, 0x76, 0x35, 0xb9, 0x0d, 0x6f, 0x23, 0x7d, 0x87, 0x9b, 0x85, 0x0c, 0xe1, 0xa1, 0x0d, 0x7f, 0x87, 0x7d, 0x84, 0x7a,
    0x84, 0x7b, 0xe1, 0x86, 0xe8, 0x6f, 0xd1, 0x79, 0x85, 0x19, 0x53, 0x95, 0xc3, 0x47, 0x19, 0x7d, 0xe7, 0x0c, 0x37, 0x7c, 0x23, 0x7d, 0x85, 0x7d, 0x4b, 0x79, 0x21, 0xa5, 0x87, 0x7d, 0x19, 0x7d,
    0x0d, 0x7d, 0x15, 0x91, 0x23, 0x7d, 0x87, 0x7c, 0x85, 0x7a, 0x85, 0xaf, 0xcd, 0x7d, 0x87, 0x7d, 0xe9, 0x3d, 0x85, 0x7d, 0x15, 0x79, 0x85, 0x7d, 0xc1, 0x7b, 0xea, 0x7d, 0xb7, 0x7d, 0x85, 0x7d,
    0x85, 0x7d, 0x0d, 0x7d, 0xe9, 0x73, 0x85, 0x79, 0x05, 0x7d, 0xd7, 0x7d, 0x85, 0xe1, 0xb9, 0xe1, 0x0f, 0x65, 0x85, 0x86, 0x2d, 0x7d, 0xd7, 0xdd, 0xa3, 0x8e, 0xe6, 0x7d, 0xde, 0x7e, 0xae, 0x0e,
    0x0f, 0xe1, 0x89, 0x7e, 0x23, 0x7d, 0xf5, 0x79, 0x23, 0xe1, 0x4b, 0x83, 0x0c, 0x0f, 0x85, 0x7b, 0x85, 0x7e, 0x8f, 0x80, 0x85, 0x98, 0xf5, 0x7a, 0x85, 0x1a, 0x0d, 0xe1, 0x0f, 0x7c, 0x89, 0x0c,
    0x85, 0x0b, 0x23, 0x69, 0x87, 0x7b, 0x23, 0x0c, 0x1f, 0xb7, 0x21, 0x7a, 0x88, 0x7e, 0x8f, 0xa5, 0x7d, 0x80, 0xb7, 0xb9, 0x18, 0xbf, 0x4b, 0x19, 0x85, 0xa5, 0x91, 0x80, 0x87, 0x81, 0x87, 0x7c,
    0x0f, 0x73, 0x91, 0x91, 0x84, 0x87, 0x37, 0xd7, 0x86, 0x79, 0xe1, 0xdd, 0x85, 0x7a, 0x73, 0x9b, 0x05, 0x7d, 0x0d, 0x83, 0x87, 0x9c, 0x85, 0x33, 0x87, 0x7d, 0x85, 0x0f, 0x87, 0x7d, 0x0d, 0x7d,
    0xf6, 0x7e, 0x87, 0x7d, 0x88, 0x19, 0x89, 0xf5, 0xd1, 0xdd, 0x85, 0x7d, 0x8b, 0xc3, 0xea, 0x7a, 0xd7, 0xb0, 0x0d, 0x7d, 0x87, 0xa5, 0x87, 0x7c, 0x73, 0x7e, 0x7d, 0x86, 0x87, 0x23, 0x85, 0x10,
    0xd7, 0xdf, 0xed, 0xa5, 0xe1, 0x7a, 0x85, 0x23, 0xea, 0x7e, 0x85, 0x98, 0xad, 0x79, 0x86, 0x7d, 0x85, 0x7d, 0xd7, 0x7d, 0xe1, 0x7a, 0xf5, 0x7d, 0x85, 0xb0, 0x2b, 0x37, 0xe1, 0x7a, 0x87, 0x79,
    0x84, 0x7d, 0x73, 0x73, 0x87, 0x7d, 0x23, 0x7d, 0xe9, 0x7d, 0x85, 0x7e, 0x02, 0x7d, 0xdd, 0x2d, 0x87, 0x79, 0xe7, 0x79, 0xad, 0x7c, 0x23, 0xda, 0x87, 0x0d, 0x0d, 0x7b, 0xe7, 0x79, 0x9b, 0x7d,
    0xd7, 0x8f, 0x05, 0x7d, 0x0d, 0x34, 0x8f, 0x7d, 0xad, 0x87, 0xe9, 0x7c, 0x85, 0x80, 0x85, 0x79, 0x8a, 0xc3, 0xe7, 0xa5, 0xe8, 0x6b, 0x0d, 0x74, 0x10, 0x73, 0x33, 0x17, 0x0d, 0x37, 0x21, 0x19
};

static int SetNonBlocking(SOCKET s) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

CPSock::CPSock() {
    Sock = 0;
    Init = 0;
    pSendBuffer = (char*)malloc(SEND_BUFFER_SIZE);
    pRecvBuffer = (char*)malloc(RECV_BUFFER_SIZE);

    memset(pSendBuffer, 0, SEND_BUFFER_SIZE);
    memset(pRecvBuffer, 0, RECV_BUFFER_SIZE);

    nSendPosition = 0;
    nSentPosition = 0;
    nRecvPosition = 0;
    nProcPosition = 0;
    memset(SendQueue, 0, MAX_KEYWORD_QUEUE);
    memset(RecvQueue, 0, MAX_KEYWORD_QUEUE);
    memset(EncodeByte, 0, sizeof(EncodeByte));
    SendCount = 0;
    RecvCount = 0;
    ErrCount = 0;
}

CPSock::~CPSock() {
    if (pSendBuffer) {
        free(pSendBuffer);
        pSendBuffer = nullptr;
    }
    if (pRecvBuffer) {
        free(pRecvBuffer);
        pRecvBuffer = nullptr;
    }
}

bool CPSock::WSAInitialize() {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(1, 1), &wsa) == 0;
#else
    return true;
#endif
}

unsigned int CPSock::StartListen(int ip, int port) {
    sockaddr_in local_sin{};
    SOCKET tSock = socket(AF_INET, SOCK_STREAM, 0);
    if (tSock == SOCKET_ERROR) {
        tmx::Log("StartListen: socket fail");
        return 0;
    }

    local_sin.sin_family = AF_INET;
    local_sin.sin_addr.s_addr = (uint32_t)ip;
    local_sin.sin_port = htons((uint16_t)port);

    if (bind(tSock, (const sockaddr*)&local_sin, sizeof(local_sin)) == SOCKET_ERROR) {
        tmx::Log("StartListen: bind fail");
        closesocket(tSock);
        return 0;
    }
    if (listen(tSock, 8) < 0) {
        tmx::Log("StartListen: listen fail");
        closesocket(tSock);
        return 0;
    }

    SetNonBlocking(tSock);
    Sock = (uintptr_t)tSock;
    return (unsigned int)tSock;
}

unsigned int CPSock::ConnectServer(const char* HostAddr, int Port, int ip) {
    nSendPosition = 0;
    nSentPosition = 0;
    nRecvPosition = 0;
    nProcPosition = 0;

    if (Sock)
        CloseSocket();

    sockaddr_in InAddr{};
    InAddr.sin_addr.s_addr = inet_addr(HostAddr);
    InAddr.sin_family = AF_INET;
    InAddr.sin_port = htons((uint16_t)Port);

    SOCKET tSock = socket(AF_INET, SOCK_STREAM, 0);
    if (tSock == SOCKET_ERROR) {
        tmx::Log("ConnectServer: socket fail");
        return 0;
    }

    sockaddr_in local_sin{};
    local_sin.sin_family = AF_INET;
    local_sin.sin_addr.s_addr = (uint32_t)ip;
    local_sin.sin_port = 0;

    if (bind(tSock, (const sockaddr*)&local_sin, sizeof(local_sin)) == SOCKET_ERROR
        && (ConnectPort += 10,
            local_sin.sin_port = htons((uint16_t)(ConnectPort + 5000)),
            bind(tSock, (const sockaddr*)&local_sin, sizeof(local_sin)) == SOCKET_ERROR)
        && (ConnectPort += 10,
            local_sin.sin_port = htons((uint16_t)(ConnectPort + 5000)),
            bind(tSock, (const sockaddr*)&local_sin, sizeof(local_sin)) == SOCKET_ERROR)) {
        tmx::Log("ConnectServer: bind fail");
        closesocket(tSock);
        return 0;
    }

    // Connect bloqueante como no original; non-blocking entra DEPOIS do handshake
    if (connect(tSock, (const sockaddr*)&InAddr, sizeof(InAddr)) < 0) {
        WSAGetLastError();
        closesocket(tSock);
        Sock = 0;
        return 0;
    }

    SetNonBlocking(tSock);
    Sock = (uintptr_t)tSock;
    unsigned int initCode = TM_INIT_CODE;
    send(tSock, (const char*)&initCode, 4, 0);
    Init = 1;
    return (unsigned int)tSock;
}

unsigned int CPSock::SingleConnect(const char* HostAddr, int Port, int ip) {
    if (Sock) {
        closesocket((SOCKET)Sock);
        Sock = 0;
    }

    sockaddr_in InAddr{};
    InAddr.sin_addr.s_addr = inet_addr(HostAddr);
    InAddr.sin_family = AF_INET;
    InAddr.sin_port = htons((uint16_t)Port);

    SOCKET tSock = socket(AF_INET, SOCK_STREAM, 0);
    if (tSock == SOCKET_ERROR) {
        tmx::Log("SingleConnect: socket fail");
        return 0;
    }

    sockaddr_in local_sin{};
    local_sin.sin_family = AF_INET;
    local_sin.sin_addr.s_addr = (uint32_t)ip;
    local_sin.sin_port = 0;

    if (bind(tSock, (const sockaddr*)&local_sin, sizeof(local_sin)) == SOCKET_ERROR
        && (ConnectPort += 10,
            local_sin.sin_port = htons((uint16_t)(ConnectPort + 5000)),
            bind(tSock, (const sockaddr*)&local_sin, sizeof(local_sin)) == SOCKET_ERROR)
        && (ConnectPort += 10,
            local_sin.sin_port = htons((uint16_t)(ConnectPort + 5000)),
            bind(tSock, (const sockaddr*)&local_sin, sizeof(local_sin)) == SOCKET_ERROR)) {
        tmx::Log("SingleConnect: bind fail");
        closesocket(tSock);
        return 0;
    }

    if (connect(tSock, (const sockaddr*)&InAddr, sizeof(InAddr)) < 0) {
        WSAGetLastError();
        closesocket(tSock);
        Sock = 0;
        return 0;
    }

    SetNonBlocking(tSock);
    Sock = (uintptr_t)tSock;
    unsigned int initCode = TM_INIT_CODE;
    send(tSock, (const char*)&initCode, 4, 0);
    Init = 1;
    return (unsigned int)tSock;
}

int CPSock::Poll() {
    if (!Sock)
        return 0;

    int r = Receive();
    if (r == -2) {
        CloseSocket();
        return -1;
    }
    if (nSentPosition < nSendPosition)
        SendMessageA();
    return 1;
}

int CPSock::Receive() {
    int rest = RECV_BUFFER_SIZE - nRecvPosition;
    if (rest <= 0)
        return -1;

    int received = recv((SOCKET)Sock, &pRecvBuffer[nRecvPosition], rest, 0);
    if (received == 0)
        return -2; // fechamento gracioso (original detecção via FD_CLOSE)
    if (received == SOCKET_ERROR) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK)
            return 0;
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
#endif
        return 0;
    }
    if (received == rest)
        return -1;

    nRecvPosition += received;
    return 1;
}

char* CPSock::ReadMessage(int* ErrorCode, int* ErrorType) {
    *ErrorCode = 0;
    if (nProcPosition >= nRecvPosition) {
        nRecvPosition = 0;
        nProcPosition = 0;
        return nullptr;
    }

    if (!Init) {
        if (nRecvPosition - nProcPosition < 4)
            return nullptr;

        unsigned int initCode = *(unsigned int*)&pRecvBuffer[nProcPosition];
        if (initCode != TM_INIT_CODE) {
            *ErrorCode = 2;
            *ErrorType = (int)initCode;
            return nullptr;
        }

        Init = 1;
        nProcPosition += 4;
    }

    if ((unsigned int)(nRecvPosition - nProcPosition) < sizeof(MSG_STANDARD))
        return nullptr;

    unsigned short size = *(unsigned short*)&pRecvBuffer[nProcPosition];
    unsigned char iKeyWord = (unsigned char)pRecvBuffer[nProcPosition + 2];
    unsigned char keyWord = pKeyWord[iKeyWord * 2];
    unsigned char checkSum = (unsigned char)pRecvBuffer[nProcPosition + 3];

    if (RecvQueue[0] != 0) {
        char qKeyword;
        if (RecvCount <= 15) {
            qKeyword = RecvQueue[RecvCount++];
        } else if (EncodeByte[0] || EncodeByte[1] || EncodeByte[2] || EncodeByte[3]) {
            char tb = EncodeByte[0];
            if (!tb)
                tb = EncodeByte[3];
            if (!tb)
                tb = 13;
            char keyword = (char)(EncodeByte[2] + EncodeByte[3] - (EncodeByte[1] * tb));
            if (EncodeByte[2] + EncodeByte[3] == tb * EncodeByte[1])
                keyword = EncodeByte[0];
            qKeyword = keyword;
        } else {
            qKeyword = (char)(RecvQueue[15] % 2
                ? RecvQueue[11] + RecvQueue[13] - RecvQueue[9] + 4
                : RecvQueue[3] + RecvQueue[1] + RecvQueue[5] - 87);
        }

        if (~qKeyword != (char)iKeyWord) {
            *ErrorCode = 3;
            *ErrorType = size;
            return nullptr;
        }
    }

    if (size >= RECV_BUFFER_SIZE || size < sizeof(MSG_STANDARD)) {
        nRecvPosition = 0;
        nProcPosition = 0;
        *ErrorCode = 2;
        *ErrorType = size;
        return nullptr;
    }

    if (size > nRecvPosition - nProcPosition)
        return nullptr;

    char* pMsg = &pRecvBuffer[nProcPosition];
    nProcPosition += size;
    if (nRecvPosition <= nProcPosition) {
        nRecvPosition = 0;
        nProcPosition = 0;
    }

    unsigned char sum1 = 0;
    unsigned char sum2 = 0;
    int pos = keyWord;
    for (int i = (int)sizeof(int); i < size; i++, pos++) {
        sum2 = (unsigned char)(sum2 + (unsigned char)pMsg[i]);

        int rst = pos % 256;
        unsigned char trans = pKeyWord[rst * 2 + 1];
        int mod = i & 0x3;

        if (mod == 0) pMsg[i] = (char)(pMsg[i] - (trans << 1));
        if (mod == 1) pMsg[i] = (char)(pMsg[i] + (trans >> 3));
        if (mod == 2) pMsg[i] = (char)(pMsg[i] - (trans << 2));
        if (mod == 3) pMsg[i] = (char)(pMsg[i] + (trans >> 5));

        sum1 = (unsigned char)(sum1 + (unsigned char)pMsg[i]);
    }

    if ((unsigned char)(sum2 - sum1) != checkSum) {
        *ErrorCode = 1;
        *ErrorType = size;
    }

    return pMsg;
}

int CPSock::CloseSocket() {
    nSendPosition = 0;
    nSentPosition = 0;
    nRecvPosition = 0;
    nProcPosition = 0;
    Init = 0;
    if (Sock)
        closesocket((SOCKET)Sock);
    Sock = 0;

    return 1;
}

int CPSock::AddMessage(char* pMsg, int Size) {
    int keyword = 0;
    if (SendQueue[0]) {
        if (SendCount > 15) {
            // branch EncodeByte do original: comentário "we don't need this now" —
            // comportamento congelado: cai no cálculo de fila abaixo
            if (SendQueue[15] % 2)
                keyword = SendQueue[11] + SendQueue[13] - SendQueue[9] + 4;
            else
                keyword = SendQueue[3] + SendQueue[1] + SendQueue[5] - 87;
            keyword ^= 0xFF;
        } else {
            keyword = SendQueue[SendCount++] ^ 0xFF;
        }
    }

    return AddMessage(pMsg, Size, keyword);
}

int CPSock::AddMessage(char* pMsg, int Size, int FixedKeyWord) {
    if (!Sock) {
        ErrCount = 10;
        return 0;
    }

    if (Size + nSendPosition < SEND_BUFFER_SIZE) {
        unsigned char iKeyWord = (unsigned char)FixedKeyWord;
        if (!FixedKeyWord)
            iKeyWord = (unsigned char)(rand() % 256);

        unsigned char keyWord = pKeyWord[iKeyWord * 2];

        auto packet = reinterpret_cast<MSG_STANDARD*>(pMsg);
        packet->Size = (uint16_t)Size;
        packet->KeyWord = (char)iKeyWord;
        packet->CheckSum = 0;

        // o original usa o CurrentTime global do jogo; no port, o relógio da plataforma
        uint32_t currentTime = tmx::GetTicks();
        packet->Tick = currentTime;

        unsigned char sum1 = 0;
        unsigned char sum2 = 0;

        int pos = keyWord;
        int i = 4;

        while (i < Size) {
            sum1 = (unsigned char)(sum1 + (unsigned char)pMsg[i]);

            int rst = pos % 256;
            unsigned char trans = pKeyWord[rst * 2 + 1];
            int mod = i & 3;

            if (!mod)
                pSendBuffer[i + nSendPosition] = (char)(pMsg[i] + 2 * trans);
            else if (mod == 1)
                pSendBuffer[i + nSendPosition] = (char)(pMsg[i] - ((int)trans >> 3));
            else if (mod == 2)
                pSendBuffer[i + nSendPosition] = (char)(pMsg[i] + 4 * trans);
            else
                pSendBuffer[i + nSendPosition] = (char)(pMsg[i] - ((int)trans >> 5));

            sum2 = (unsigned char)(sum2 + (unsigned char)pSendBuffer[i++ + nSendPosition]);
            ++pos;
        }

        unsigned char checkSum = (unsigned char)(sum2 - sum1);
        packet->CheckSum = (char)checkSum;
        memcpy(&pSendBuffer[nSendPosition], pMsg, 4u);

        nSendPosition += Size;

        SendMessageA();
        return 1;
    }

    ErrCount = 1;
    return 0;
}

bool CPSock::SendMessageA() {
    if (!Sock) {
        nSendPosition = 0;
        nSentPosition = 0;
        return false;
    }

    if (nSentPosition > 0)
        RefreshSendBuffer();

    if (nSendPosition <= SEND_BUFFER_SIZE && nSendPosition >= 0) {
        if (nSentPosition > nSendPosition || nSentPosition >= SEND_BUFFER_SIZE || nSentPosition < 0) {
            tmx::Log("err, send2 %d %d %d", nSendPosition, nSentPosition, (int)Sock);
            nSendPosition = 0;
            nSentPosition = 0;
        }

        for (int i = 0; i < 1; ++i) {
            int sent = send((SOCKET)Sock, &pSendBuffer[nSentPosition], nSendPosition - nSentPosition, 0);
            if (sent == SOCKET_ERROR)
                WSAGetLastError();
            else
                nSentPosition += sent;

            if (nSentPosition >= nSendPosition && sent != SOCKET_ERROR) {
                nSendPosition = 0;
                nSentPosition = 0;
                return true;
            }
        }

        return nSendPosition < SEND_BUFFER_SIZE;
    }

    tmx::Log("err, send1 %d %d %d", nSendPosition, nSentPosition, (int)Sock);
    nSendPosition = 0;
    nSentPosition = 0;
    return false;
}

int CPSock::SendOneMessage(char* Msg, int Size) {
    AddMessage(Msg, Size);
    return SendMessageA();
}

int CPSock::SendOneMessageKeyword(char* Msg, int Size, int Keyword) {
    AddMessage(Msg, Size, Keyword);
    return SendMessageA();
}

int CPSock::AddMessage2(char* pMsg, int Size) {
    if (!Sock)
        return 0;

    if (Size + nSendPosition > SEND_BUFFER_SIZE)
        return 0;

    memcpy(&pSendBuffer[nSendPosition], pMsg, Size);
    nSendPosition += Size;

    return 1;
}

char* CPSock::ReadMessage2(int* ErrorCode, int* ErrorType) {
    *ErrorCode = 0;

    if (nProcPosition >= nRecvPosition) {
        nRecvPosition = 0;
        nProcPosition = 0;
    } else if ((unsigned int)(nRecvPosition - nProcPosition) >= 12) {
        char* pMsg = &pRecvBuffer[nProcPosition];
        nProcPosition += *(uint16_t*)pMsg;

        if (nRecvPosition <= nProcPosition) {
            nRecvPosition = 0;
            nProcPosition = 0;
        }

        return pMsg;
    }

    return nullptr;
}

void CPSock::RefreshRecvBuffer() {
    int left = nRecvPosition - nProcPosition;

    if (left > 0 && left <= RECV_BUFFER_SIZE) {
        memcpy(pRecvBuffer, &pRecvBuffer[nProcPosition], left);

        nProcPosition = 0;
        nRecvPosition -= left;
    }
}

void CPSock::RefreshSendBuffer() {
    int left = nSendPosition - nSentPosition;

    if (left > 0 && left <= RECV_BUFFER_SIZE) {
        // BUG DO ORIGINAL preservado por fidelidade: lê de pRecvBuffer (CPSock.cpp:658).
        // Na prática nunca corrompe porque quem chama (SendMessageA) já enviou tudo.
        // TODO(pós-paridade): corrigir para &pSendBuffer[nSentPosition].
        memcpy(pSendBuffer, &pRecvBuffer[nSentPosition], left);

        nSentPosition = 0;
        nSendPosition -= left;
    }
}
