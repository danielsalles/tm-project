#include "test_framework.h"

#include "math/TMMath.h"

#include <cstdio>
#include <cstring>

// Tolerâncias (política do doc 14-fase0-fundacao.md §5.3):
//   mul/add puros      → ~exato (1e-6 abs folga para reassociação/FMA)
//   com sqrt/divisão   → 1e-5
//   inverse / slerp    → 1e-3 / 1e-4
//   intersect hit/miss → EXATO (bool); u/v/dist 1e-5
static const float TOL_EXACT = 1e-6f;
static const float TOL_SQRT  = 1e-5f;
static const float TOL_INV   = 1e-3f;
static const float TOL_SLERP = 1e-4f;

static FILE* OpenGolden(const char* name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", TM_GOLDEN_DIR, name);
    return fopen(path, "rb");
}

static float MaxAbsDiff(const float* a, const float* b, int n) {
    float m = 0;
    for (int i = 0; i < n; i++)
        m = (std::max)(m, fabsf(a[i] - b[i]));
    return m;
}

// Erro relativo à escala dos valores dourados: abs(a-b) / max(1, max|b|).
// Necessário porque as entradas têm magnitude de mundo (±20000) — float32 tem
// granularidade ~2^-8 nessa escala, então tolerância absoluta não faz sentido.
static float MaxScaledDiff(const float* a, const float* b, int n) {
    float num = 0, den = 1.0f;
    for (int i = 0; i < n; i++) {
        num = (std::max)(num, fabsf(a[i] - b[i]));
        den = (std::max)(den, fabsf(b[i]));
    }
    return num / den;
}

#define GOLDEN_OR_SKIP(name) \
    FILE* f = OpenGolden(name); \
    if (!f) { printf("  SKIP %s (golden ausente — rodar generate-golden)\n", name); return; }

// ---------------- golden: matrix_multiply.bin [A B R] × N ----------------
TEST(tmmath, golden_multiply) {
    GOLDEN_OR_SKIP("matrix_multiply.bin");
    float maxErr = 0;
    int cases = 0;
    for (;;) {
        D3DXMATRIX A, B, R;
        if (fread(&A, 64, 1, f) != 1) break;
        if (fread(&B, 64, 1, f) != 1) break;
        if (fread(&R, 64, 1, f) != 1) break;
        D3DXMATRIX r;
        D3DXMatrixMultiply(&r, &A, &B);
        maxErr = (std::max)(maxErr, MaxScaledDiff(&r.m[0][0], &R.m[0][0], 16));
        cases++;
    }
    fclose(f);
    printf("  %d casos, maxErr=%g\n", cases, maxErr);
    EXPECT_TRUE(cases > 0);
    EXPECT_LE(maxErr, 1e-6f);
}

// ---------------- golden: matrix_create.bin [kind][4 params][R] × N ----------------
TEST(tmmath, golden_matrix_create) {
    GOLDEN_OR_SKIP("matrix_create.bin");
    float maxErr = 0;
    int cases = 0;
    for (;;) {
        uint32_t kind;
        float p[4];
        D3DXMATRIX R;
        if (fread(&kind, 4, 1, f) != 1) break;
        if (fread(p, 16, 1, f) != 1) break;
        if (fread(&R, 64, 1, f) != 1) break;
        D3DXMATRIX r;
        switch (kind) {
        case 0: D3DXMatrixTranslation(&r, p[0], p[1], p[2]); break;
        case 1: D3DXMatrixScaling(&r, p[0], p[1], p[2]); break;
        case 2: D3DXMatrixRotationX(&r, p[0]); break;
        case 3: D3DXMatrixRotationY(&r, p[0]); break;
        case 4: D3DXMatrixRotationZ(&r, p[0]); break;
        case 5: D3DXMatrixRotationYawPitchRoll(&r, p[0], p[1], p[2]); break;
        case 6: { D3DXVECTOR3 axis(p[0], p[1], p[2]); D3DXMatrixRotationAxis(&r, &axis, p[3]); break; }
        default: continue;
        }
        maxErr = (std::max)(maxErr, MaxScaledDiff(&r.m[0][0], &R.m[0][0], 16));
        cases++;
    }
    fclose(f);
    printf("  %d casos, maxErr=%g\n", cases, maxErr);
    EXPECT_TRUE(cases > 0);
    EXPECT_LE(maxErr, 1e-6f); // rotations usam sinf/cosf
}

// ---------------- golden: lookat.bin / perspective.bin ----------------
TEST(tmmath, golden_lookat) {
    GOLDEN_OR_SKIP("lookat.bin");
    float maxErr = 0;
    int cases = 0;
    for (;;) {
        D3DXVECTOR3 eye, at, up;
        D3DXMATRIX R;
        if (fread(&eye, 12, 1, f) != 1) break;
        if (fread(&at, 12, 1, f) != 1) break;
        if (fread(&up, 12, 1, f) != 1) break;
        if (fread(&R, 64, 1, f) != 1) break;
        D3DXMATRIX r;
        D3DXMatrixLookAtLH(&r, &eye, &at, &up);
        maxErr = (std::max)(maxErr, MaxScaledDiff(&r.m[0][0], &R.m[0][0], 16));
        cases++;
    }
    fclose(f);
    printf("  %d casos, maxErr=%g\n", cases, maxErr);
    EXPECT_TRUE(cases > 0);
    EXPECT_LE(maxErr, 1e-6f);
}

TEST(tmmath, golden_perspective) {
    GOLDEN_OR_SKIP("perspective.bin");
    float maxErr = 0;
    int cases = 0;
    for (;;) {
        float p[4];
        D3DXMATRIX R;
        if (fread(p, 16, 1, f) != 1) break;
        if (fread(&R, 64, 1, f) != 1) break;
        D3DXMATRIX r;
        D3DXMatrixPerspectiveFovLH(&r, p[0], p[1], p[2], p[3]);
        maxErr = (std::max)(maxErr, MaxScaledDiff(&r.m[0][0], &R.m[0][0], 16));
        cases++;
    }
    fclose(f);
    printf("  %d casos, maxErr=%g\n", cases, maxErr);
    EXPECT_TRUE(cases > 0);
    EXPECT_LE(maxErr, 1e-5f); // tanf diverge ~1-2 ULP entre CRTs
}

// ---------------- golden: inverse.bin [A][R][det] × N ----------------
TEST(tmmath, golden_inverse) {
    GOLDEN_OR_SKIP("inverse.bin");
    float maxErr = 0;
    int cases = 0;
    for (;;) {
        D3DXMATRIX A, R;
        float det;
        if (fread(&A, 64, 1, f) != 1) break;
        if (fread(&R, 64, 1, f) != 1) break;
        if (fread(&det, 4, 1, f) != 1) break;
        D3DXMATRIX r;
        float d;
        EXPECT_TRUE(D3DXMatrixInverse(&r, &d, &A) != nullptr);
        maxErr = (std::max)(maxErr, MaxScaledDiff(&r.m[0][0], &R.m[0][0], 16));
        EXPECT_NEAR(d, det, TOL_INV * (fabsf(det) + 1.0f));
        cases++;
    }
    fclose(f);
    printf("  %d casos, maxErr=%g\n", cases, maxErr);
    EXPECT_TRUE(cases > 0);
    EXPECT_LE(maxErr, 1e-4f);
}

// ---------------- golden: transform.bin [v][M][T][TC] × N ----------------
TEST(tmmath, golden_transform) {
    GOLDEN_OR_SKIP("transform.bin");
    float maxErr4 = 0, maxErrC = 0;
    int cases = 0;
    for (;;) {
        D3DXVECTOR3 v;
        D3DXMATRIX M;
        D3DXVECTOR4 T;
        D3DXVECTOR3 TC;
        if (fread(&v, 12, 1, f) != 1) break;
        if (fread(&M, 64, 1, f) != 1) break;
        if (fread(&T, 16, 1, f) != 1) break;
        if (fread(&TC, 12, 1, f) != 1) break;
        D3DXVECTOR4 t;
        D3DXVec3Transform(&t, &v, &M);
        D3DXVECTOR3 tc;
        D3DXVec3TransformCoord(&tc, &v, &M);
        maxErr4 = (std::max)(maxErr4, MaxScaledDiff(&t.x, &T.x, 4));
        maxErrC = (std::max)(maxErrC, MaxScaledDiff(&tc.x, &TC.x, 3));
        cases++;
    }
    fclose(f);
    printf("  %d casos, maxErr4=%g maxErrC=%g\n", cases, maxErr4, maxErrC);
    EXPECT_TRUE(cases > 0);
    EXPECT_LE(maxErr4, 1e-6f);
    EXPECT_LE(maxErrC, 1e-5f);
}

// ---------------- golden: quaternion.bin [q1][q2][t][slerp][m][qback] × N ----------------
TEST(tmmath, golden_quaternion) {
    GOLDEN_OR_SKIP("quaternion.bin");
    float maxSlerp = 0, maxMat = 0, maxBack = 0;
    int cases = 0;
    for (;;) {
        D3DXQUATERNION q1, q2, S, QBACK;
        D3DXMATRIX M;
        float t;
        if (fread(&q1, 16, 1, f) != 1) break;
        if (fread(&q2, 16, 1, f) != 1) break;
        if (fread(&t, 4, 1, f) != 1) break;
        if (fread(&S, 16, 1, f) != 1) break;
        if (fread(&M, 64, 1, f) != 1) break;
        if (fread(&QBACK, 16, 1, f) != 1) break;

        D3DXQUATERNION s;
        D3DXQuaternionSlerp(&s, &q1, &q2, t);
        maxSlerp = (std::max)(maxSlerp, MaxAbsDiff(&s.x, &S.x, 4));

        D3DXMATRIX m;
        D3DXMatrixRotationQuaternion(&m, &q1);
        maxMat = (std::max)(maxMat, MaxAbsDiff(&m.m[0][0], &M.m[0][0], 16));

        // q e -q são a mesma rotação — comparar por valor absoluto do dot
        D3DXQUATERNION qb;
        D3DXQuaternionRotationMatrix(&qb, &M);
        float dot = fabsf(qb.x * QBACK.x + qb.y * QBACK.y + qb.z * QBACK.z + qb.w * QBACK.w);
        maxBack = (std::max)(maxBack, fabsf(1.0f - dot));
        cases++;
    }
    fclose(f);
    printf("  %d casos, slerp=%g mat=%g back=%g\n", cases, maxSlerp, maxMat, maxBack);
    EXPECT_TRUE(cases > 0);
    EXPECT_LE(maxSlerp, TOL_SLERP);
    EXPECT_LE(maxMat, TOL_SQRT);
    EXPECT_LE(maxBack, TOL_SLERP);
}

// ---------------- golden: intersect.bin [p0p1p2][pos dir][hit][u v dist] × N ----------------
TEST(tmmath, golden_intersect) {
    GOLDEN_OR_SKIP("intersect.bin");
    float maxErr = 0;
    int cases = 0, hitMismatches = 0;
    for (;;) {
        D3DXVECTOR3 p0, p1, p2, pos, dir;
        uint32_t hit;
        float u, v, dist;
        if (fread(&p0, 12, 1, f) != 1) break;
        if (fread(&p1, 12, 1, f) != 1) break;
        if (fread(&p2, 12, 1, f) != 1) break;
        if (fread(&pos, 12, 1, f) != 1) break;
        if (fread(&dir, 12, 1, f) != 1) break;
        if (fread(&hit, 4, 1, f) != 1) break;
        if (fread(&u, 4, 1, f) != 1) break;
        if (fread(&v, 4, 1, f) != 1) break;
        if (fread(&dist, 4, 1, f) != 1) break;

        float uu = 0, vv = 0, dd = 0;
        BOOL h = D3DXIntersectTri(&p0, &p1, &p2, &pos, &dir, &uu, &vv, &dd);
        if ((h != FALSE) != (hit != 0)) {
            hitMismatches++;
            printf("  hit mismatch caso %d: d3d=%u shim=%d\n", cases, hit, (int)h);
        }
        if (h && hit) {
            maxErr = (std::max)(maxErr, fabsf(uu - u));
            maxErr = (std::max)(maxErr, fabsf(vv - v));
            maxErr = (std::max)(maxErr, fabsf(dd - dist));
        }
        cases++;
    }
    fclose(f);
    printf("  %d casos, hitMismatches=%d, maxErr=%g\n", cases, hitMismatches, maxErr);
    EXPECT_TRUE(cases > 0);
    EXPECT_EQ(hitMismatches, 0); // sinal hit/miss tem de ser idêntico — picking depende
    EXPECT_LE(maxErr, 1e-4f); // u,v∈[0,1] e dist~100: 1e-4 abs ≈ 1e-6 rel
}

// ---------------- golden: project.bin [v][vp][world][view][proj][out] × N ----------------
TEST(tmmath, golden_project) {
    GOLDEN_OR_SKIP("project.bin");
    float maxErr = 0;
    int cases = 0;
    for (;;) {
        D3DXVECTOR3 v, out;
        D3DVIEWPORT9 vp;
        D3DXMATRIX world, view, proj;
        if (fread(&v, 12, 1, f) != 1) break;
        if (fread(&vp, 24, 1, f) != 1) break;
        if (fread(&world, 64, 1, f) != 1) break;
        if (fread(&view, 64, 1, f) != 1) break;
        if (fread(&proj, 64, 1, f) != 1) break;
        if (fread(&out, 12, 1, f) != 1) break;
        D3DXVECTOR3 o;
        D3DXVec3Project(&o, &v, &vp, &proj, &view, &world);
        maxErr = (std::max)(maxErr, MaxScaledDiff(&o.x, &out.x, 3));
        cases++;
    }
    fclose(f);
    printf("  %d casos, maxErr=%g\n", cases, maxErr);
    EXPECT_TRUE(cases > 0);
    EXPECT_LE(maxErr, 1e-2f);
}

// ================= autoconsistência (sempre roda, mesmo sem dourados) =================

TEST(tmmath, identity_multiply) {
    D3DXMATRIX I, A, R;
    D3DXMatrixIdentity(&I);
    D3DXMatrixRotationYawPitchRoll(&A, 0.7f, -0.3f, 1.2f);
    A._41 = 100.0f; A._42 = -50.0f; A._43 = 20000.0f;
    D3DXMatrixMultiply(&R, &I, &A);
    EXPECT_LE(MaxAbsDiff(&R.m[0][0], &A.m[0][0], 16), 0.0f);
    D3DXMatrixMultiply(&R, &A, &I);
    EXPECT_LE(MaxAbsDiff(&R.m[0][0], &A.m[0][0], 16), 0.0f);
}

TEST(tmmath, translation_applies_to_origin) {
    D3DXMATRIX M;
    D3DXMatrixTranslation(&M, 5, -3, 7);
    D3DXVECTOR3 o(0, 0, 0), r;
    D3DXVec3TransformCoord(&r, &o, &M);
    EXPECT_NEAR(r.x, 5.0f, TOL_EXACT);
    EXPECT_NEAR(r.y, -3.0f, TOL_EXACT);
    EXPECT_NEAR(r.z, 7.0f, TOL_EXACT);
}

TEST(tmmath, ypr_equals_rz_rx_ry) {
    // D3DX aplica roll(Z), pitch(X), yaw(Y) nessa ordem
    D3DXVECTOR3 v(1, 2, 3), a, b;
    D3DXMATRIX YPR, Z, X, Y, T1, T2;
    D3DXMatrixRotationYawPitchRoll(&YPR, 0.5f, 0.3f, 0.8f);
    D3DXMatrixRotationZ(&Z, 0.8f);
    D3DXMatrixRotationX(&X, 0.3f);
    D3DXMatrixRotationY(&Y, 0.5f);
    D3DXMatrixMultiply(&T1, &Z, &X);
    D3DXMatrixMultiply(&T2, &T1, &Y);
    D3DXVec3TransformCoord(&a, &v, &YPR);
    D3DXVec3TransformCoord(&b, &v, &T2);
    EXPECT_LE(MaxAbsDiff(&a.x, &b.x, 3), TOL_EXACT);
}

TEST(tmmath, lookat_eye_maps_to_origin_at_maps_to_plus_z) {
    D3DXVECTOR3 eye(10, 20, 30), at(11, 20, 35), up(0, 1, 0);
    D3DXMATRIX V;
    D3DXMatrixLookAtLH(&V, &eye, &at, &up);
    D3DXVECTOR3 r, zero(0, 0, 0);
    D3DXVec3TransformCoord(&r, &eye, &V);
    EXPECT_LE(MaxAbsDiff(&r.x, &zero.x, 3), TOL_SQRT);
    D3DXVec3TransformCoord(&r, &at, &V);
    EXPECT_NEAR(r.x, 0.0f, TOL_SQRT);
    EXPECT_NEAR(r.y, 0.0f, TOL_SQRT);
    EXPECT_TRUE(r.z > 0.0f); // LH: à frente = +z
}

TEST(tmmath, perspective_known_values) {
    D3DXMATRIX P;
    // fov do jogo: 0.25 * 180 graus em rad = π/4; aspect 4/3; near 0.966; far 70
    D3DXMatrixPerspectiveFovLH(&P, 3.14159265f / 4.0f, 4.0f / 3.0f, 0.966f, 70.0f);
    float yScale = 1.0f / tanf(3.14159265f / 8.0f);
    EXPECT_NEAR(P._22, yScale, TOL_EXACT);
    EXPECT_NEAR(P._11, yScale / (4.0f / 3.0f), TOL_EXACT);
    EXPECT_NEAR(P._33, 70.0f / (70.0f - 0.966f), TOL_EXACT);
    EXPECT_NEAR(P._34, 1.0f, 0.0f);
    EXPECT_NEAR(P._43, -0.966f * 70.0f / (70.0f - 0.966f), TOL_EXACT);
}

TEST(tmmath, inverse_roundtrip) {
    D3DXMATRIX A, Inv, R, I;
    D3DXMatrixRotationYawPitchRoll(&A, 1.1f, 0.2f, -0.7f);
    A._41 = 123.0f; A._42 = -456.0f; A._43 = 789.0f;
    float det;
    EXPECT_TRUE(D3DXMatrixInverse(&Inv, &det, &A) != nullptr);
    D3DXMatrixMultiply(&R, &A, &Inv);
    D3DXMatrixIdentity(&I);
    float err = MaxAbsDiff(&R.m[0][0], &I.m[0][0], 16);
    printf("  roundtrip err=%g\n", err);
    // autoconsistência float32 varia por compilador/SIMD; a fidelidade ao D3DX
    // é cobrada no golden_inverse (TOL_INV relativo). Aqui basta sanidade.
    EXPECT_LE(err, 1e-3f);
}

TEST(tmmath, slerp_endpoints_and_halfway) {
    D3DXQUATERNION q1, q2, r;
    D3DXQuaternionIdentity(&q1);
    D3DXMATRIX M;
    D3DXMatrixRotationY(&M, 3.14159265f / 2.0f);
    D3DXQuaternionRotationMatrix(&q2, &M);

    D3DXQuaternionSlerp(&r, &q1, &q2, 0.0f);
    EXPECT_NEAR(r.w, q1.w, TOL_SLERP);
    D3DXQuaternionSlerp(&r, &q1, &q2, 1.0f);
    EXPECT_NEAR(fabsf(r.w - q2.w) + fabsf(r.x - q2.x) + fabsf(r.y - q2.y) + fabsf(r.z - q2.z),
                0.0f, TOL_SLERP);

    // meio do caminho de 0→90° em Y = 45° em Y
    // convenção LH row-major do D3DX: (1,0,0) → (cos, 0, -sin)
    D3DXQuaternionSlerp(&r, &q1, &q2, 0.5f);
    D3DXMATRIX RM;
    D3DXMatrixRotationQuaternion(&RM, &r);
    D3DXVECTOR3 v(1, 0, 0), rv;
    D3DXVec3TransformCoord(&rv, &v, &RM);
    float s45 = 0.70710678f;
    EXPECT_NEAR(rv.x, s45, 1e-3f);
    EXPECT_NEAR(rv.z, -s45, 1e-3f);
}

TEST(tmmath, quaternion_matrix_roundtrip) {
    D3DXQUATERNION q1(0.18257419f, 0.36514837f, -0.54772256f, 0.73029674f); // normalizado
    D3DXMATRIX M;
    D3DXMatrixRotationQuaternion(&M, &q1);
    D3DXQUATERNION q2;
    D3DXQuaternionRotationMatrix(&q2, &M);
    float dot = fabsf(q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w);
    EXPECT_NEAR(dot, 1.0f, TOL_SLERP);
}

TEST(tmmath, intersect_basic) {
    D3DXVECTOR3 p0(0, 0, 0), p1(1, 0, 0), p2(0, 1, 0);
    D3DXVECTOR3 pos(0.25f, 0.25f, 1), dir(0, 0, -1);
    float u, v, d;
    EXPECT_TRUE(D3DXIntersectTri(&p0, &p1, &p2, &pos, &dir, &u, &v, &d));
    EXPECT_NEAR(d, 1.0f, TOL_SQRT);

    // paralelo → miss
    D3DXVECTOR3 dirPar(1, 0, 0);
    EXPECT_FALSE(D3DXIntersectTri(&p0, &p1, &p2, &pos, &dirPar, &u, &v, &d));

    // fora do triângulo → miss
    D3DXVECTOR3 posOut(2, 2, 1);
    EXPECT_FALSE(D3DXIntersectTri(&p0, &p1, &p2, &posOut, &dir, &u, &v, &d));

    // backface → HIT (D3DX não faz culling — picking do terreno depende disso)
    D3DXVECTOR3 posBack(0.25f, 0.25f, -1), dirBack(0, 0, 1);
    EXPECT_TRUE(D3DXIntersectTri(&p0, &p1, &p2, &posBack, &dirBack, &u, &v, &d));
}
