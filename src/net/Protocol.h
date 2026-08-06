#pragma once

#include <cstdint>

// Header de protocolo de pacotes (12 bytes, packing natural — sem padding).
// Copiado de Basedef.h; o resto das mensagens migra junto com o gameplay.
struct MSG_STANDARD {
    uint16_t Size;
    char KeyWord;
    char CheckSum;
    uint16_t Type;
    uint16_t ID;
    uint32_t Tick;
};

constexpr uint32_t TM_INIT_CODE = 521270033;
