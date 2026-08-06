// golden_generator.cpp — gera os valores dourados com o D3DX9 REAL.
// Compilar UMA VEZ no Windows (o job generate-golden do CI faz isso):
//   cl /EHsc golden_generator.cpp /I Dependencies\Directx\Include
//      /link /LIBPATH:Dependencies\Directx\Lib d3dx9.lib
// Uso: golden_generator.exe <out_dir>
// Formato de cada arquivo: registros binários float32 crus, sem header.
// Layouts documentados em tests/test_tmmath.cpp (leitores correspondentes).

#include <d3dx9.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

static uint32_t s_seed = 0x12345678;
static float frand(float lo, float hi) { // LCG próprio — determinístico entre CRTs
    s_seed = s_seed * 1664525u + 1013904223u;
    float t = (float)(s_seed >> 8) / 16777216.0f; // [0,1)
    return lo + t * (hi - lo);
}

static void FillRandomMatrix(D3DXMATRIX* m) {
    // matriz "de jogo": rotação * translação com escala de mundo (±20000)
    D3DXMATRIX rot;
    D3DXMatrixRotationYawPitchRoll(&rot, frand(-3.14f, 3.14f), frand(-1.5f, 1.5f), frand(-3.14f, 3.14f));
    D3DXMATRIX tra;
    D3DXMatrixTranslation(&tra, frand(-20000.0f, 20000.0f), frand(-200.0f, 500.0f), frand(-20000.0f, 20000.0f));
    D3DXMatrixMultiply(m, &rot, &tra);
}

static void W(FILE* f, const void* p, size_t n) { fwrite(p, 1, n, f); }

static char g_dir[512] = ".";
static void SetDir(const char* d) { strcpy(g_dir, d); }

static FILE* OpenOut(const char* name) {
    char path[600];
    snprintf(path, sizeof(path), "%s/%s", g_dir, name);
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "falha ao abrir %s\n", path); exit(1); }
    return f;
}

#define GEN_LOOP(name, n, body) \
    { FILE* f = OpenOut(name); for (int i = 0; i < (n); i++) { body } fclose(f); }

int main(int argc, char** argv) {
    if (argc > 1)
        SetDir(argv[1]);

    const int N = 256;

    // --- matrix_multiply.bin: [A B R] × N ---
    GEN_LOOP("matrix_multiply.bin", N, {
        D3DXMATRIX A, B, R;
        FillRandomMatrix(&A); FillRandomMatrix(&B);
        D3DXMatrixMultiply(&R, &A, &B);
        W(f, &A, 64); W(f, &B, 64); W(f, &R, 64);
    });

    // --- matrix_create.bin: [kind u32][p0..p3][R] × (N + especiais) ---
    GEN_LOOP("matrix_create.bin", N + 6, {
        uint32_t kind;
        float p[4] = {};
        D3DXMATRIX R;
        if (i >= N) {
            // casos especiais: ângulos 0, pi, -pi/2, etc.
            static const float special[6][4] = {
                {0, 0, 0, 0}, {3.14159265f, 0, 0, 0}, {-1.5707963f, 0, 0, 0},
                {6.2831853f, 1.5707963f, -3.14159265f, 0},
                {0.0f, 0.0f, 0.0f, 0}, {1, 0, 0, 0.7071f},
            };
            kind = (uint32_t)(2 + (i - N) % 5);
            memcpy(p, special[i - N], 16);
        } else {
            kind = (uint32_t)(i % 7);
            p[0] = frand(-6.28f, 6.28f); p[1] = frand(-6.28f, 6.28f);
            p[2] = frand(-6.28f, 6.28f); p[3] = frand(-6.28f, 6.28f);
        }
        switch (kind) {
        case 0: D3DXMatrixTranslation(&R, p[0], p[1], p[2]); break;
        case 1: D3DXMatrixScaling(&R, p[0], p[1], p[2]); break;
        case 2: D3DXMatrixRotationX(&R, p[0]); break;
        case 3: D3DXMatrixRotationY(&R, p[0]); break;
        case 4: D3DXMatrixRotationZ(&R, p[0]); break;
        case 5: D3DXMatrixRotationYawPitchRoll(&R, p[0], p[1], p[2]); break;
        default: { D3DXVECTOR3 axis(p[0], p[1], p[2]); D3DXMatrixRotationAxis(&R, &axis, p[3]); break; }
        }
        W(f, &kind, 4); W(f, p, 16); W(f, &R, 64);
    });

    // --- lookat.bin: [eye at up R] × N ---
    GEN_LOOP("lookat.bin", N, {
        D3DXVECTOR3 eye(frand(-20000.0f, 20000.0f), frand(10.0f, 300.0f), frand(-20000.0f, 20000.0f));
        D3DXVECTOR3 at = eye + D3DXVECTOR3(frand(-10.0f, 10.0f), frand(-3.0f, 3.0f), frand(1.0f, 10.0f));
        D3DXVECTOR3 up(0, 1, 0);
        D3DXMATRIX R;
        D3DXMatrixLookAtLH(&R, &eye, &at, &up);
        W(f, &eye, 12); W(f, &at, 12); W(f, &up, 12); W(f, &R, 64);
    });

    // --- perspective.bin: [fovY aspect zn zf][R] × N ---
    GEN_LOOP("perspective.bin", N, {
        float p[4];
        D3DXMATRIX R;
        if (i == 0) { // parâmetros exatos do jogo
            p[0] = 0.25f * 3.14159265f; p[1] = 4.0f / 3.0f; p[2] = 0.966f; p[3] = 70.0f;
        } else {
            p[0] = frand(0.1f, 2.5f); p[1] = frand(0.5f, 3.0f);
            p[2] = frand(0.01f, 2.0f); p[3] = frand(50.0f, 500.0f);
        }
        D3DXMatrixPerspectiveFovLH(&R, p[0], p[1], p[2], p[3]);
        W(f, p, 16); W(f, &R, 64);
    });

    // --- inverse.bin: [A R det] × N ---
    GEN_LOOP("inverse.bin", N, {
        D3DXMATRIX A, R;
        float det = 0;
        FillRandomMatrix(&A);
        if (D3DXMatrixInverse(&R, &det, &A) == nullptr) { i--; continue; }
        W(f, &A, 64); W(f, &R, 64); W(f, &det, 4);
    });

    // --- transform.bin: [v M T TC] × N ---
    GEN_LOOP("transform.bin", N, {
        D3DXVECTOR3 v(frand(-100.0f, 100.0f), frand(-100.0f, 100.0f), frand(-100.0f, 100.0f));
        D3DXMATRIX M;
        FillRandomMatrix(&M);
        D3DXVECTOR4 T;
        D3DXVECTOR3 TC;
        D3DXVec3Transform(&T, &v, &M);
        D3DXVec3TransformCoord(&TC, &v, &M);
        W(f, &v, 12); W(f, &M, 64); W(f, &T, 16); W(f, &TC, 12);
    });

    // --- quaternion.bin: [q1 q2 t slerp m qback] × N ---
    GEN_LOOP("quaternion.bin", N, {
        D3DXMATRIX m1, m2;
        FillRandomMatrix(&m1); FillRandomMatrix(&m2);
        // zerar translação (só rotação interessa para quaternion)
        m1._41 = m1._42 = m1._43 = 0; m2._41 = m2._42 = m2._43 = 0;
        D3DXQUATERNION q1, q2;
        D3DXQuaternionRotationMatrix(&q1, &m1);
        D3DXQuaternionRotationMatrix(&q2, &m2);
        float t;
        if (i == 0) { q2 = q1; t = 0.5f; }                    // mesmo quat
        else if (i == 1) { q2.x=-q1.x; q2.y=-q1.y; q2.z=-q1.z; q2.w=-q1.w; t = 0.5f; } // oposto
        else t = frand(0.0f, 1.0f);
        D3DXQUATERNION S;
        D3DXQuaternionSlerp(&S, &q1, &q2, t);
        D3DXMATRIX M;
        D3DXMatrixRotationQuaternion(&M, &q1);
        D3DXQUATERNION QB;
        D3DXQuaternionRotationMatrix(&QB, &M);
        W(f, &q1, 16); W(f, &q2, 16); W(f, &t, 4); W(f, &S, 16); W(f, &M, 64); W(f, &QB, 16);
    });

    // --- intersect.bin: [p0 p1 p2 pos dir hit u v dist] × N ---
    GEN_LOOP("intersect.bin", N, {
        D3DXVECTOR3 p0(frand(-50.0f, 50.0f), frand(-50.0f, 50.0f), frand(-50.0f, 50.0f));
        D3DXVECTOR3 p1 = p0 + D3DXVECTOR3(frand(-20.0f, 20.0f), frand(-20.0f, 20.0f), frand(-20.0f, 20.0f));
        D3DXVECTOR3 p2 = p0 + D3DXVECTOR3(frand(-20.0f, 20.0f), frand(-20.0f, 20.0f), frand(-20.0f, 20.0f));
        D3DXVECTOR3 pos, dir;
        if (i % 4 == 0) { // garantido hit: origem dentro do triângulo, raio na normal
            D3DXVECTOR3 c = (p0 + p1 + p2) / 3.0f;
            D3DXVECTOR3 e1 = p1 - p0, e2 = p2 - p0, n;
            D3DXVec3Cross(&n, &e1, &e2);
            D3DXVec3Normalize(&n, &n);
            pos = c + n * frand(1.0f, 100.0f);
            dir = n * -1.0f;
        } else if (i % 4 == 1) { // paralelo → miss
            pos = p0 + D3DXVECTOR3(0, 0, frand(1.0f, 10.0f));
            D3DXVECTOR3 e1 = p1 - p0;
            D3DXVec3Normalize(&dir, &e1);
        } else if (i % 4 == 2) { // atrás do raio
            D3DXVECTOR3 c = (p0 + p1 + p2) / 3.0f;
            D3DXVECTOR3 e1 = p1 - p0, e2 = p2 - p0, n;
            D3DXVec3Cross(&n, &e1, &e2);
            D3DXVec3Normalize(&n, &n);
            pos = c + n * frand(1.0f, 100.0f);
            dir = n; // apontando para longe
        } else {
            pos = D3DXVECTOR3(frand(-50.0f, 50.0f), frand(-50.0f, 50.0f), frand(-50.0f, 50.0f));
            dir = D3DXVECTOR3(frand(-1.0f, 1.0f), frand(-1.0f, 1.0f), frand(-1.0f, 1.0f));
        }
        float u = 0, v = 0, dist = 0;
        uint32_t hit = D3DXIntersectTri(&p0, &p1, &p2, &pos, &dir, &u, &v, &dist) ? 1u : 0u;
        W(f, &p0, 12); W(f, &p1, 12); W(f, &p2, 12); W(f, &pos, 12); W(f, &dir, 12);
        W(f, &hit, 4); W(f, &u, 4); W(f, &v, 4); W(f, &dist, 4);
    });

    // --- project.bin: [v vp world view proj out] × N ---
    GEN_LOOP("project.bin", N, {
        D3DVIEWPORT9 vp;
        vp.X = 0; vp.Y = 0; vp.Width = 1024; vp.Height = 768; vp.MinZ = 0.0f; vp.MaxZ = 1.0f;
        D3DXMATRIX world, view, proj;
        D3DXMatrixIdentity(&world);
        D3DXVECTOR3 eye(frand(-1000.0f, 1000.0f), frand(10.0f, 100.0f), frand(-1000.0f, 1000.0f));
        D3DXVECTOR3 at = eye + D3DXVECTOR3(frand(-5.0f, 5.0f), 0, frand(1.0f, 5.0f));
        D3DXVECTOR3 up(0, 1, 0);
        D3DXMatrixLookAtLH(&view, &eye, &at, &up);
        D3DXMatrixPerspectiveFovLH(&proj, 0.25f * 3.14159265f, 4.0f / 3.0f, 0.966f, 70.0f);
        D3DXVECTOR3 v = eye + D3DXVECTOR3(frand(-30.0f, 30.0f), frand(-5.0f, 5.0f), frand(1.0f, 60.0f));
        D3DXVECTOR3 out;
        D3DXVec3Project(&out, &v, &vp, &proj, &view, &world);
        W(f, &v, 12); W(f, &vp, 24); W(f, &world, 64); W(f, &view, 64); W(f, &proj, 64); W(f, &out, 12);
    });

    printf("golden data gerada em %s\n", g_dir);
    return 0;
}
