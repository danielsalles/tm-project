#pragma once

// CPSock — port do cliente de rede (Winsock/WSAAsyncSelect → BSD non-blocking).
// Diferenças em relação ao original (Projects/TMProject/CPSock.cpp):
//   - WSAAsyncSelect(HWND, msg) substituído por sockets non-blocking + Poll() por frame
//   - MessageBoxA → tmx::Log
//   - CurrentTime global do jogo → tmx::GetTicks() (o jogo plugará seu relógio depois)
//   - EncodeByte (global de Basedef.h) → membro
// A lógica de encode/checksum/queues é byte-a-byte idêntica à original.

#include "net/Protocol.h"

#include <cstddef>

#define RECV_BUFFER_SIZE 131072
#define SEND_BUFFER_SIZE 131072
#define MAX_KEYWORD_QUEUE 16

class CPSock {
public:
    CPSock();
    ~CPSock();

    bool WSAInitialize();
    unsigned int StartListen(int ip, int port);
    unsigned int ConnectServer(const char* HostAddr, int Port, int ip);
    unsigned int SingleConnect(const char* HostAddr, int Port, int ip);

    // Chamar 1× por frame: drena recv, flusha envio pendente.
    // Retorno: 1 ok, 0 sem socket, -1 conexão fechada pelo remoto.
    int Poll();

    int Receive();
    char* ReadMessage(int* ErrorCode, int* ErrorType);
    int CloseSocket();
    int AddMessage(char* pMsg, int Size);
    int AddMessage(char* pMsg, int Size, int FixedKeyWord);
    bool SendMessageA();
    int SendOneMessage(char* Msg, int Size);
    int SendOneMessageKeyword(char* Msg, int Size, int Keyword);
    int AddMessage2(char* pMsg, int Size);
    char* ReadMessage2(int* ErrorCode, int* ErrorType);
    void RefreshRecvBuffer();
    void RefreshSendBuffer();

    uintptr_t Sock;
    char* pSendBuffer;
    char* pRecvBuffer;
    int nSendPosition;
    int nRecvPosition;
    int nProcPosition;
    int nSentPosition;
    int Init;

    char SendQueue[MAX_KEYWORD_QUEUE];
    char RecvQueue[MAX_KEYWORD_QUEUE];

    int SendCount;
    int RecvCount;
    int ErrCount;

    char EncodeByte[4];
};

extern unsigned char pKeyWord[512];
