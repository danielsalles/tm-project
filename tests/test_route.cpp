#include "test_framework.h"

#include "world/Route.h"

#include <cstring>
#include <vector>

using tmx::GetRoute;
using tmx::RouteMask;
using tmx::RouteStepDir;

namespace {

RouteMask FlatMask(std::vector<int8_t>& storage, int w = 32, int h = 32) {
    storage.assign((size_t)w * h, 10);  // flat height 10 everywhere
    RouteMask m;
    m.cells = storage.data();
    m.width = w;
    m.height = h;
    return m;
}

}

TEST(route, straight_east_reaches_target) {
    std::vector<int8_t> cells;
    RouteMask m = FlatMask(cells);
    uint8_t route[24];
    int tx = 10, ty = 5;
    EXPECT_EQ(GetRoute(m, 5, 5, &tx, &ty, route, 12, 8), 1);
    EXPECT_EQ(tx, 10);
    EXPECT_EQ(ty, 5);
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(route[i], 54);
    EXPECT_EQ(route[5], 0);
}

TEST(route, diagonal_northeast) {
    std::vector<int8_t> cells;
    RouteMask m = FlatMask(cells);
    uint8_t route[24];
    int tx = 8, ty = 2;   // from (5,5): 3 steps ++x --y
    EXPECT_EQ(GetRoute(m, 5, 5, &tx, &ty, route, 12, 8), 1);
    EXPECT_EQ(tx, 8);
    EXPECT_EQ(ty, 2);
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(route[i], 51);
}

TEST(route, wall_stops_before_blocked_cell) {
    std::vector<int8_t> cells;
    RouteMask m = FlatMask(cells);
    // Wall at x=8: huge height delta (> MH) on the whole column.
    for (int y = 0; y < 32; ++y)
        cells[8 + 32 * y] = 100;
    uint8_t route[24];
    int tx = 12, ty = 5;
    EXPECT_EQ(GetRoute(m, 5, 5, &tx, &ty, route, 12, 8), 1);
    EXPECT_TRUE(tx < 8);         // stopped before the wall
    EXPECT_EQ(tx, 7);
    EXPECT_EQ(ty, 5);
    // 2 clean steps (5->6->7); at 7 the direct/diagonal moves are all blocked.
    EXPECT_EQ(route[0], 54);
    EXPECT_EQ(route[1], 54);
    EXPECT_EQ(route[2], 0);
}

TEST(route, unreachable_adjacent_target_stops_immediately) {
    std::vector<int8_t> cells;
    RouteMask m = FlatMask(cells);
    cells[8 + 32 * 5] = 100;      // target cell itself is blocked
    uint8_t route[24];
    int tx = 8, ty = 5;
    // From (7,5): e is blocked; "adjacent target" rule -> no movement at all.
    EXPECT_EQ(GetRoute(m, 7, 5, &tx, &ty, route, 12, 8), 0);
    EXPECT_EQ(route[0], 0);
}

TEST(route, gentle_slope_within_tolerance) {
    std::vector<int8_t> cells;
    RouteMask m = FlatMask(cells);
    for (int x = 0; x < 32; ++x)
        for (int y = 0; y < 32; ++y)
            cells[x + 32 * y] = (int8_t)(10 + (x - 5));  // +1 per cell east
    uint8_t route[24];
    int tx = 10, ty = 5;
    EXPECT_EQ(GetRoute(m, 5, 5, &tx, &ty, route, 12, 8), 1);
    EXPECT_EQ(tx, 10);
}

TEST(route, already_at_target) {
    std::vector<int8_t> cells;
    RouteMask m = FlatMask(cells);
    uint8_t route[24];
    int tx = 5, ty = 5;
    EXPECT_EQ(GetRoute(m, 5, 5, &tx, &ty, route, 12, 8), 0);
}

TEST(route, step_dir_mapping) {
    int dx, dy;
    RouteStepDir(49, dx, dy); EXPECT_TRUE(dx == -1 && dy == -1);
    RouteStepDir(50, dx, dy); EXPECT_TRUE(dx == 0 && dy == -1);
    RouteStepDir(51, dx, dy); EXPECT_TRUE(dx == 1 && dy == -1);
    RouteStepDir(52, dx, dy); EXPECT_TRUE(dx == -1 && dy == 0);
    RouteStepDir(54, dx, dy); EXPECT_TRUE(dx == 1 && dy == 0);
    RouteStepDir(55, dx, dy); EXPECT_TRUE(dx == -1 && dy == 1);
    RouteStepDir(56, dx, dy); EXPECT_TRUE(dx == 0 && dy == 1);
    RouteStepDir(57, dx, dy); EXPECT_TRUE(dx == 1 && dy == 1);
}
