#include "world/Route.h"

#include <cstring>

namespace tmx {

int GetRoute(const RouteMask& m, int x, int y, int* tx, int* ty,
             uint8_t route[24], int distance, int mh) {
    const int lastx = x;
    const int lasty = y;
    int targetX = *tx;
    int targetY = *ty;
    memset(route, 0, 24);

    const int W = m.width;
    const int H = m.height;
    const int8_t* p = m.cells;
    if (!p)
        return 0;

    for (int i = 0; i < distance && i < 23; ++i) {
        if (x < 1 || y < 1 || x > W - 2 || y > H - 2) {
            route[i] = 0;
            break;
        }

        const int cul = p[x + W * y];
        const int n   = p[x + W * (y - 1)];
        const int ne  = p[x + 1 + W * (y - 1)];
        const int e   = p[x + 1 + W * y];
        const int se  = p[x + 1 + W * (y + 1)];
        const int s   = p[x + W * (y + 1)];
        const int sw  = p[x - 1 + W * (y + 1)];
        const int w   = p[x - 1 + W * y];
        const int nw  = p[x - 1 + W * (y - 1)];

        if (targetX == x && targetY == y) {
            route[i] = 0;
            break;
        }
        if (targetX == x && targetY > y && s < mh + cul && s > cul - mh) {
            route[i] = 56; ++y;
        } else if (targetX == x && targetY < y && n < mh + cul && n > cul - mh) {
            route[i] = 50; --y;
        } else if (targetX > x && targetY < y && ne < mh + cul && ne > cul - mh
                   && (n < mh + cul && n > cul - mh || e < mh + cul && e > cul - mh)) {
            route[i] = 51; ++x; --y;
        } else if (targetX > x && targetY == y && e < mh + cul && e > cul - mh) {
            route[i] = 54; ++x;
        } else if (targetX > x && targetY > y && se < mh + cul && se > cul - mh
                   && (s < mh + cul && s > cul - mh || e < mh + cul && e > cul - mh)) {
            route[i] = 57; ++x; ++y;
        } else if (targetX < x && targetY > y && sw < mh + cul && sw > cul - mh
                   && (s < mh + cul && s > cul - mh || w < mh + cul && w > cul - mh)) {
            route[i] = 55; --x; ++y;
        } else if (targetX < x && targetY == y && w < mh + cul && w > cul - mh) {
            route[i] = 52; --x;
        } else if (targetX < x && targetY < y && nw < mh + cul && nw > cul - mh
                   && (n < mh + cul && n > cul - mh || w < mh + cul && w > cul - mh)) {
            route[i] = 49; --x; --y;
        } else if (targetX > x && targetY < y && e < mh + cul && e > cul - mh) {
            route[i] = 54; ++x;
        } else if (targetX > x && targetY < y && n < mh + cul && n > cul - mh) {
            route[i] = 50; --y;
        } else if (targetX > x && targetY > y && e < mh + cul && e > cul - mh) {
            route[i] = 54; ++x;
        } else if (targetX > x && targetY > y && s < mh + cul && s > cul - mh) {
            route[i] = 56; ++y;
        } else if (targetX < x && targetY > y && w < mh + cul && w > cul - mh) {
            route[i] = 52; --x;
        } else if (targetX < x && targetY > y && s < mh + cul && s > cul - mh) {
            route[i] = 56; ++y;
        } else if (targetX < x && targetY < y && w < mh + cul && w > cul - mh) {
            route[i] = 52; --x;
        } else if (targetX < x && targetY < y && n < mh + cul && n > cul - mh) {
            route[i] = 50; --y;
        } else {
            if (targetX == x + 1 || targetY == y + 1 || targetX == x - 1 || targetY == y - 1) {
                route[i] = 0;
                break;
            }
            if (targetX == x && targetY > y && se < mh + cul && se > cul - mh
                && (s < mh + cul && s > cul - mh || e < mh + cul && e > cul - mh)) {
                route[i] = 57; ++x; ++y;
            } else if (targetX == x && targetY > y && sw < mh + cul && sw > cul - mh
                       && (s < mh + cul && s > cul - mh || w < mh + cul && w > cul - mh)) {
                route[i] = 55; --x; ++y;
            } else if (targetX == x && targetY < y && ne < mh + cul && ne > cul - mh
                       && (n < mh + cul && n > cul - mh || e < mh + cul && e > cul - mh)) {
                route[i] = 51; ++x; --y;
            } else if (targetX == x && targetY < y && nw < mh + cul && nw > cul - mh
                       && (n < mh + cul && n > cul - mh || w < mh + cul && w > cul - mh)) {
                route[i] = 49; --x; --y;
            } else if (targetX < x && targetY == y && sw < mh + cul && sw > cul - mh
                       && (s < mh + cul && s > cul - mh || w < mh + cul && w > cul - mh)) {
                route[i] = 55; --x; ++y;
            } else if (targetX < x && targetY == y && nw < mh + cul && nw > cul - mh
                       && (n < mh + cul && n > cul - mh || w < mh + cul && w > cul - mh)) {
                route[i] = 49; --x; --y;
            } else if (targetX > x && targetY == y && se < mh + cul && se > cul - mh
                       && (s < mh + cul && s > cul - mh || e < mh + cul && e > cul - mh)) {
                route[i] = 57; ++x; ++y;
            } else {
                if (targetX <= x || targetY != y || ne >= mh + cul || ne <= cul - mh
                    || (n >= mh + cul || n <= cul - mh) && (e >= mh + cul || e <= cul - mh)) {
                    route[i] = 0;
                    break;
                }
                route[i] = 51; ++x; --y;
            }
        }
    }

    if (lastx == x && lasty == y)
        return 0;

    *tx = x;
    *ty = y;
    return 1;
}

}
