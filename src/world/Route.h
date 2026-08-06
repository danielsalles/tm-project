#pragma once

#include <cstdint>

namespace tmx {

// BASE_GetRoute (Basedef.cpp:859-1090): greedy step-by-step routing over the
// scene height/attribute mask. Coordinates are mask cells (caller converts
// world -> cell). Route codes are numpad directions (49..57, 53 unused);
// 0 terminates. Returns 0 when no step could be taken; otherwise *tx/*ty hold
// the reached cell (the "walkable target").
struct RouteMask {
    const int8_t* cells = nullptr;  // width*height, row-major; 127 = blocked
    int width = 128;
    int height = 128;
};

int GetRoute(const RouteMask& m, int x, int y, int* tx, int* ty,
             uint8_t route[24], int distance, int mh);

inline void RouteStepDir(uint8_t code, int& dx, int& dy) {
    const int c = code - 49;
    dx = (c % 3) - 1;
    dy = (c / 3) - 1;
}

}
