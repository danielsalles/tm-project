#pragma once

// HttpClient — minimal blocking HTTP/1.0 GET replacing WinInet (doc 21 §6).
//
// The original has three call sites, all plain-HTTP GETs of small payloads
// (BASE_GetHttpRequest helper x2, Guildmark_Download 632-byte BMP). No HTTPS,
// no redirects, no chunked encoding — the era's servers are static files.
// If HTTPS ever becomes necessary (patch/login, phase 8) the swap is
// localized to this file.

namespace tmx {

struct HttpUrl {
    char host[128]{};
    int port = 80;
    char path[256] = "/";
    bool ok = false;
};

// Parses "http://host[:port]/path" (pure — unit tested).
HttpUrl ParseHttpUrl(const char* url);

// Blocking GET. On success returns the body length (>0) with the body
// NUL-terminated at outBuf; returns 0 on any failure. Response headers are
// stripped. Body is capped at outCap-1. `timeoutMs` bounds connect+recv.
int HttpGet(const char* url, char* outBuf, int outCap, int timeoutMs = 5000);

// BASE_GetHttpRequest parity (Basedef.cpp:396-424).
int BaseGetHttpRequest(const char* url, char* outBuf, int outCap);

}
