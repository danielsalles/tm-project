#include "test_framework.h"

#include "world/LookResolver.h"

#include <cstring>

using tmx::LookInput;
using tmx::LookPart;
using tmx::ResolveLookParts;

namespace {

LookInput Make(const char* prefix, int idx = 0) {
    LookInput in;
    in.boneAniIndex = idx;
    strncpy(in.prefix, prefix, sizeof in.prefix - 1);
    return in;
}

}

TEST(look, base_rule_ch01) {
    LookInput in = Make("mesh\\ch01", 0);
    LookPart out[8];
    ResolveLookParts(in, out);
    // look all zero: part i -> ch01<i+1>01.msh / ch01<i+1>01.wyt
    EXPECT_TRUE(!strcmp(out[0].mesh, "mesh\\ch010101.msh"));
    EXPECT_TRUE(!strcmp(out[0].tex, "mesh\\ch010101.wyt"));
    EXPECT_TRUE(!strcmp(out[1].mesh, "mesh\\ch010201.msh"));
    EXPECT_TRUE(!strcmp(out[5].tex, "mesh\\ch010601.wyt"));
    for (int i = 0; i < 8; ++i)
        EXPECT_TRUE(out[i].visible);
}

TEST(look, skin_adds_to_texture_variant) {
    LookInput in = Make("mesh\\ch01", 0);
    in.meshLook[0] = 2;   // mesh ch010103
    in.skinLook[0] = 5;   // tex  variant 5+2+1 = 8
    LookPart out[8];
    ResolveLookParts(in, out);
    EXPECT_TRUE(!strcmp(out[0].mesh, "mesh\\ch010103.msh"));
    EXPECT_TRUE(!strcmp(out[0].tex, "mesh\\ch010108.wyt"));
}

TEST(look, fixed_variant_types) {
    LookInput in = Make("mesh\\LB01", 45);
    in.meshLook[2] = 7;
    in.skinLook[2] = 3;
    LookPart out[8];
    ResolveLookParts(in, out);
    EXPECT_TRUE(!strcmp(out[2].mesh, "mesh\\LB010301.msh"));
    EXPECT_TRUE(!strcmp(out[2].tex, "mesh\\LB010301.wyt"));
}

TEST(look, visibility_hide_rule) {
    LookInput in = Make("mesh\\ch01", 0);
    in.meshLook[0] = 90;   // "hide equipment" flag
    in.meshLook[2] = 0;    // empty slot -> hidden
    in.meshLook[3] = 1;    // non-empty -> still visible
    LookPart out[8];
    ResolveLookParts(in, out);
    EXPECT_TRUE(out[0].visible);    // face always visible
    EXPECT_TRUE(!out[2].visible);
    EXPECT_TRUE(out[3].visible);
}

TEST(look, exception_ch010218_texture) {
    LookInput in = Make("mesh\\ch01", 0);
    in.meshLook[1] = 17;          // mesh ch010218
    in.skinLook[1] = 1;           // tex variant 1+17+1 = 19
    LookPart out[8];
    ResolveLookParts(in, out);
    EXPECT_TRUE(!strcmp(out[1].mesh, "mesh\\ch010218.msh"));
    EXPECT_TRUE(!strcmp(out[1].tex, "mesh\\ch010214.wyt"));
}

TEST(look, exception_mantle_mesh_swap) {
    LookInput in = Make("mesh\\mt01", 85);
    in.meshLook[0] = 0;
    in.skinLook[0] = 169;         // tex variant 170 -> mt0101170
    LookPart out[8];
    ResolveLookParts(in, out);
    EXPECT_TRUE(!strcmp(out[0].tex, "mesh\\mt0101170.wyt"));
    EXPECT_TRUE(!strcmp(out[0].mesh, "mesh\\mt010131.msh"));
}

TEST(look, exception_mt010124_self_mesh) {
    LookInput in = Make("mesh\\mt01", 85);
    in.skinLook[0] = 23;          // tex variant 24 -> mt010124
    LookPart out[8];
    ResolveLookParts(in, out);
    EXPECT_TRUE(!strcmp(out[0].mesh, "mesh\\mt010124.msh"));
}

TEST(look, exception_ch02_to_ch01_redirects) {
    LookInput in = Make("mesh\\ch02", 1);
    in.meshLook[0] = 0;           // part 01 -> [10]=='1'
    in.skinLook[0] = 129;         // variant 130 -> ch0201130
    LookPart out[8];
    ResolveLookParts(in, out);
    EXPECT_TRUE(!strcmp(out[0].tex, "mesh\\ch010130.wyt"));

    in.meshLook[3] = 0;           // part 04 -> [10]=='4'
    in.skinLook[3] = 129;
    ResolveLookParts(in, out);
    EXPECT_TRUE(!strcmp(out[3].tex, "mesh\\ch010430.wyt"));

    in.meshLook[4] = 0;           // part 05 -> [10]=='5'
    in.skinLook[4] = 129;
    ResolveLookParts(in, out);
    EXPECT_TRUE(!strcmp(out[4].tex, "mesh\\ch010530.wyt"));
}

TEST(look, exception_simple_swaps) {
    LookInput in = Make("mesh\\ch02", 1);
    in.meshLook[2] = 2;           // part 03
    in.skinLook[2] = 12;          // variant 15 -> ch020315
    LookPart out[8];
    ResolveLookParts(in, out);
    EXPECT_TRUE(!strcmp(out[2].tex, "mesh\\ch020314.wyt"));

    LookInput bm = Make("mesh\\bm01", 8);
    bm.meshLook[0] = 0;
    bm.skinLook[0] = 1;           // variant 2 -> bm010102
    ResolveLookParts(bm, out);
    EXPECT_TRUE(!strcmp(out[0].tex, "mesh\\mi010105.wyt"));
}

TEST(look, exception_tr_aliases) {
    LookInput in = Make("mesh\\tr19", 82);
    in.meshLook[0] = 0;
    in.skinLook[0] = 0;           // variant 1 -> tr190101
    LookPart out[8];
    ResolveLookParts(in, out);
    EXPECT_TRUE(!strcmp(out[0].tex, "mesh\\tr180101.wyt"));

    in.skinLook[0] = 1;           // tr190102
    ResolveLookParts(in, out);
    EXPECT_TRUE(!strcmp(out[0].tex, "mesh\\tr180102.wyt"));
}

TEST(look, god2_single_texture) {
    LookInput in = Make("mesh\\be01", 29);  // 'b','e' -> God2
    in.meshLook[3] = 2;
    in.skinLook[3] = 4;
    LookPart out[8];
    ResolveLookParts(in, out);
    // mesh follows the part index; texture pinned to part 01 with variant 4+2+1=7.
    EXPECT_TRUE(!strcmp(out[3].mesh, "mesh\\be010403.msh"));
    EXPECT_TRUE(!strcmp(out[3].tex, "mesh\\be010107.wyt"));
}
