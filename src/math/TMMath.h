#pragma once

// TMMath — shim header-only da API D3DX usada pelo cliente.
// Convenções preservadas (NÃO "corrigir"):
//   - row-major, vetor-linha: v' = v * M; M = A * B aplica A primeiro
//   - left-handed: z cresce "para dentro" da tela
//   - z de clip/NDC em [0, 1] (corrigido para GL no vertex shader, ver 04-convencoes.md)
// Validado contra golden tests gerados com o D3DX9 real (tests/golden/).

#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
typedef int BOOL;
typedef uint32_t D3DCOLOR;

#define D3DXToRadian(degree) ((degree) * (3.141592654f / 180.0f))
#define D3DXToDegree(radian) ((radian) * (180.0f / 3.141592654f))

#define D3DCOLOR_ARGB(a,r,g,b) \
    ((D3DCOLOR)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))
#define D3DCOLOR_RGBA(r,g,b,a) D3DCOLOR_ARGB(a,r,g,b)
#define D3DCOLOR_XRGB(r,g,b)   D3DCOLOR_ARGB(0xff,r,g,b)

struct D3DVIEWPORT9 {
    uint32_t X, Y, Width, Height;
    float MinZ, MaxZ;
};

struct D3DXVECTOR2 {
    float x, y;
    D3DXVECTOR2() : x(0), y(0) {}
    D3DXVECTOR2(float _x, float _y) : x(_x), y(_y) {}
    D3DXVECTOR2 operator+(const D3DXVECTOR2& v) const { return {x + v.x, y + v.y}; }
    D3DXVECTOR2 operator-(const D3DXVECTOR2& v) const { return {x - v.x, y - v.y}; }
    D3DXVECTOR2 operator*(float s) const { return {x * s, y * s}; }
    D3DXVECTOR2 operator/(float s) const { return {x / s, y / s}; }
    D3DXVECTOR2& operator+=(const D3DXVECTOR2& v) { x += v.x; y += v.y; return *this; }
    D3DXVECTOR2& operator-=(const D3DXVECTOR2& v) { x -= v.x; y -= v.y; return *this; }
    D3DXVECTOR2& operator*=(float s) { x *= s; y *= s; return *this; }
};

struct D3DXVECTOR3 {
    float x, y, z;
    D3DXVECTOR3() : x(0), y(0), z(0) {}
    D3DXVECTOR3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    D3DXVECTOR3 operator+(const D3DXVECTOR3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    D3DXVECTOR3 operator-(const D3DXVECTOR3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    D3DXVECTOR3 operator-() const { return {-x, -y, -z}; }
    D3DXVECTOR3 operator*(float s) const { return {x * s, y * s, z * s}; }
    D3DXVECTOR3 operator/(float s) const { return {x / s, y / s, z / s}; }
    D3DXVECTOR3& operator+=(const D3DXVECTOR3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    D3DXVECTOR3& operator-=(const D3DXVECTOR3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    D3DXVECTOR3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    D3DXVECTOR3& operator/=(float s) { x /= s; y /= s; z /= s; return *this; }
    bool operator==(const D3DXVECTOR3& v) const { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const D3DXVECTOR3& v) const { return !(*this == v); }
};

struct D3DXVECTOR4 {
    float x, y, z, w;
    D3DXVECTOR4() : x(0), y(0), z(0), w(0) {}
    D3DXVECTOR4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};

struct D3DXQUATERNION {
    float x, y, z, w;
    D3DXQUATERNION() : x(0), y(0), z(0), w(1) {}
    D3DXQUATERNION(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};

struct D3DXCOLOR {
    float r, g, b, a;
    D3DXCOLOR() : r(0), g(0), b(0), a(0) {}
    D3DXCOLOR(float _r, float _g, float _b, float _a) : r(_r), g(_g), b(_b), a(_a) {}
    D3DXCOLOR(D3DCOLOR c)
        : r(((c >> 16) & 0xff) / 255.0f), g(((c >> 8) & 0xff) / 255.0f),
          b((c & 0xff) / 255.0f), a(((c >> 24) & 0xff) / 255.0f) {}
};

struct D3DXMATRIX {
    union {
        struct {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
    };
    D3DXMATRIX() {}
    D3DXMATRIX(const float* f) { memcpy(m, f, sizeof(m)); }
    float& operator()(int row, int col) { return m[row][col]; }
    float operator()(int row, int col) const { return m[row][col]; }
    D3DXMATRIX operator*(const D3DXMATRIX& o) const;
};

struct alignas(16) D3DXMATRIXA16 : D3DXMATRIX {
    D3DXMATRIXA16() {}
    D3DXMATRIXA16(const float* f) : D3DXMATRIX(f) {}
};

// ---------------- matriz: criação ----------------

inline D3DXMATRIX* D3DXMatrixIdentity(D3DXMATRIX* out) {
    out->m[0][0] = 1; out->m[0][1] = 0; out->m[0][2] = 0; out->m[0][3] = 0;
    out->m[1][0] = 0; out->m[1][1] = 1; out->m[1][2] = 0; out->m[1][3] = 0;
    out->m[2][0] = 0; out->m[2][1] = 0; out->m[2][2] = 1; out->m[2][3] = 0;
    out->m[3][0] = 0; out->m[3][1] = 0; out->m[3][2] = 0; out->m[3][3] = 1;
    return out;
}

inline D3DXMATRIX* D3DXMatrixMultiply(D3DXMATRIX* out, const D3DXMATRIX* a, const D3DXMATRIX* b) {
    D3DXMATRIX r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            r.m[i][j] = a->m[i][0] * b->m[0][j] + a->m[i][1] * b->m[1][j]
                      + a->m[i][2] * b->m[2][j] + a->m[i][3] * b->m[3][j];
    *out = r;
    return out;
}

inline D3DXMATRIX D3DXMATRIX::operator*(const D3DXMATRIX& o) const {
    D3DXMATRIX r;
    D3DXMatrixMultiply(&r, this, &o);
    return r;
}

inline D3DXMATRIX* D3DXMatrixTranspose(D3DXMATRIX* out, const D3DXMATRIX* in) {
    D3DXMATRIX r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            r.m[i][j] = in->m[j][i];
    *out = r;
    return out;
}

inline D3DXMATRIX* D3DXMatrixMultiplyTranspose(D3DXMATRIX* out, const D3DXMATRIX* a, const D3DXMATRIX* b) {
    D3DXMatrixMultiply(out, a, b);
    return D3DXMatrixTranspose(out, out);
}

inline D3DXMATRIX* D3DXMatrixTranslation(D3DXMATRIX* out, float x, float y, float z) {
    D3DXMatrixIdentity(out);
    out->_41 = x; out->_42 = y; out->_43 = z;
    return out;
}

inline D3DXMATRIX* D3DXMatrixScaling(D3DXMATRIX* out, float x, float y, float z) {
    D3DXMatrixIdentity(out);
    out->_11 = x; out->_22 = y; out->_33 = z;
    return out;
}

// Rotações LH row-major (formas do D3DX — não converter para RH aqui)
inline D3DXMATRIX* D3DXMatrixRotationX(D3DXMATRIX* out, float angle) {
    D3DXMatrixIdentity(out);
    float c = cosf(angle), s = sinf(angle);
    out->_22 = c;  out->_23 = s;
    out->_32 = -s; out->_33 = c;
    return out;
}

inline D3DXMATRIX* D3DXMatrixRotationY(D3DXMATRIX* out, float angle) {
    D3DXMatrixIdentity(out);
    float c = cosf(angle), s = sinf(angle);
    out->_11 = c;  out->_13 = -s;
    out->_31 = s;  out->_33 = c;
    return out;
}

inline D3DXMATRIX* D3DXMatrixRotationZ(D3DXMATRIX* out, float angle) {
    D3DXMatrixIdentity(out);
    float c = cosf(angle), s = sinf(angle);
    out->_11 = c;  out->_12 = s;
    out->_21 = -s; out->_22 = c;
    return out;
}

// D3DX: aplica roll(Z), depois pitch(X), depois yaw(Y) → M = Rz * Rx * Ry
// (ordem de aplicação = ordem do produto na convenção row-major vetor-linha)
inline D3DXMATRIX* D3DXMatrixRotationYawPitchRoll(D3DXMATRIX* out, float yaw, float pitch, float roll) {
    D3DXMATRIX rz, rx, ry, t;
    D3DXMatrixRotationZ(&rz, roll);
    D3DXMatrixRotationX(&rx, pitch);
    D3DXMatrixRotationY(&ry, yaw);
    D3DXMatrixMultiply(&t, &rz, &rx);
    return D3DXMatrixMultiply(out, &t, &ry);
}

// Rodrigues LH row-major — sinais de `s` validados pelo golden test
inline D3DXMATRIX* D3DXMatrixRotationAxis(D3DXMATRIX* out, const D3DXVECTOR3* axis, float angle) {
    D3DXVECTOR3 n = *axis;
    float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len > 0.0f) { n.x /= len; n.y /= len; n.z /= len; }
    float c = cosf(angle), s = sinf(angle), t = 1.0f - c;
    out->_11 = t * n.x * n.x + c;     out->_12 = t * n.x * n.y + s * n.z; out->_13 = t * n.x * n.z - s * n.y; out->_14 = 0;
    out->_21 = t * n.x * n.y - s * n.z; out->_22 = t * n.y * n.y + c;     out->_23 = t * n.y * n.z + s * n.x; out->_24 = 0;
    out->_31 = t * n.x * n.z + s * n.y; out->_32 = t * n.y * n.z - s * n.x; out->_33 = t * n.z * n.z + c;     out->_34 = 0;
    out->_41 = 0; out->_42 = 0; out->_43 = 0; out->_44 = 1;
    return out;
}

inline D3DXMATRIX* D3DXMatrixLookAtLH(D3DXMATRIX* out, const D3DXVECTOR3* eye,
                                      const D3DXVECTOR3* at, const D3DXVECTOR3* up) {
    D3DXVECTOR3 zaxis = *at - *eye;
    float zl = sqrtf(zaxis.x * zaxis.x + zaxis.y * zaxis.y + zaxis.z * zaxis.z);
    zaxis = zaxis / zl;
    D3DXVECTOR3 xaxis(up->y * zaxis.z - up->z * zaxis.y,
                      up->z * zaxis.x - up->x * zaxis.z,
                      up->x * zaxis.y - up->y * zaxis.x);
    float xl = sqrtf(xaxis.x * xaxis.x + xaxis.y * xaxis.y + xaxis.z * xaxis.z);
    xaxis = xaxis / xl;
    D3DXVECTOR3 yaxis(zaxis.y * xaxis.z - zaxis.z * xaxis.y,
                      zaxis.z * xaxis.x - zaxis.x * xaxis.z,
                      zaxis.x * xaxis.y - zaxis.y * xaxis.x);
    out->_11 = xaxis.x; out->_12 = yaxis.x; out->_13 = zaxis.x; out->_14 = 0;
    out->_21 = xaxis.y; out->_22 = yaxis.y; out->_23 = zaxis.y; out->_24 = 0;
    out->_31 = xaxis.z; out->_32 = yaxis.z; out->_33 = zaxis.z; out->_34 = 0;
    out->_41 = -(xaxis.x * eye->x + xaxis.y * eye->y + xaxis.z * eye->z);
    out->_42 = -(yaxis.x * eye->x + yaxis.y * eye->y + yaxis.z * eye->z);
    out->_43 = -(zaxis.x * eye->x + zaxis.y * eye->y + zaxis.z * eye->z);
    out->_44 = 1;
    return out;
}

inline D3DXMATRIX* D3DXMatrixPerspectiveFovLH(D3DXMATRIX* out, float fovY, float aspect,
                                              float zn, float zf) {
    float yScale = 1.0f / tanf(fovY * 0.5f);
    float xScale = yScale / aspect;
    out->m[0][0] = xScale; out->m[0][1] = 0;       out->m[0][2] = 0;                out->m[0][3] = 0;
    out->m[1][0] = 0;      out->m[1][1] = yScale;  out->m[1][2] = 0;                out->m[1][3] = 0;
    out->m[2][0] = 0;      out->m[2][1] = 0;       out->m[2][2] = zf / (zf - zn);   out->m[2][3] = 1;
    out->m[3][0] = 0;      out->m[3][1] = 0;       out->m[3][2] = -zn * zf / (zf - zn); out->m[3][3] = 0;
    return out;
}

// ---------------- vetores ----------------

inline float D3DXVec2Length(const D3DXVECTOR2* v) { return sqrtf(v->x * v->x + v->y * v->y); }

inline D3DXVECTOR2* D3DXVec2Normalize(D3DXVECTOR2* out, const D3DXVECTOR2* v) {
    float len = D3DXVec2Length(v);
    if (len > 0.0f) { out->x = v->x / len; out->y = v->y / len; }
    else { out->x = 0; out->y = 0; }
    return out;
}

inline float D3DXVec3Length(const D3DXVECTOR3* v) { return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z); }

inline float D3DXVec3Dot(const D3DXVECTOR3* a, const D3DXVECTOR3* b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

inline D3DXVECTOR3* D3DXVec3Cross(D3DXVECTOR3* out, const D3DXVECTOR3* a, const D3DXVECTOR3* b) {
    D3DXVECTOR3 r(a->y * b->z - a->z * b->y,
                  a->z * b->x - a->x * b->z,
                  a->x * b->y - a->y * b->x);
    *out = r;
    return out;
}

inline D3DXVECTOR3* D3DXVec3Normalize(D3DXVECTOR3* out, const D3DXVECTOR3* v) {
    float len = D3DXVec3Length(v);
    if (len > 0.0f) { out->x = v->x / len; out->y = v->y / len; out->z = v->z / len; }
    else { out->x = 0; out->y = 0; out->z = 0; }
    return out;
}

inline D3DXVECTOR3* D3DXVec3Lerp(D3DXVECTOR3* out, const D3DXVECTOR3* a, const D3DXVECTOR3* b, float t) {
    out->x = a->x + t * (b->x - a->x);
    out->y = a->y + t * (b->y - a->y);
    out->z = a->z + t * (b->z - a->z);
    return out;
}

// v' = v * M (4D, sem divisão)
inline D3DXVECTOR4* D3DXVec3Transform(D3DXVECTOR4* out, const D3DXVECTOR3* v, const D3DXMATRIX* m) {
    D3DXVECTOR4 r(v->x * m->_11 + v->y * m->_21 + v->z * m->_31 + m->_41,
                  v->x * m->_12 + v->y * m->_22 + v->z * m->_32 + m->_42,
                  v->x * m->_13 + v->y * m->_23 + v->z * m->_33 + m->_43,
                  v->x * m->_14 + v->y * m->_24 + v->z * m->_34 + m->_44);
    *out = r;
    return out;
}

// v' = (v * M) / w
inline D3DXVECTOR3* D3DXVec3TransformCoord(D3DXVECTOR3* out, const D3DXVECTOR3* v, const D3DXMATRIX* m) {
    D3DXVECTOR4 t;
    D3DXVec3Transform(&t, v, m);
    float w = t.w;
    D3DXVECTOR3 r(t.x / w, t.y / w, t.z / w);
    *out = r;
    return out;
}

inline D3DXVECTOR4* D3DXVec4Transform(D3DXVECTOR4* out, const D3DXVECTOR4* v, const D3DXMATRIX* m) {
    D3DXVECTOR4 r(v->x * m->_11 + v->y * m->_21 + v->z * m->_31 + v->w * m->_41,
                  v->x * m->_12 + v->y * m->_22 + v->z * m->_32 + v->w * m->_42,
                  v->x * m->_13 + v->y * m->_23 + v->z * m->_33 + v->w * m->_43,
                  v->x * m->_14 + v->y * m->_24 + v->z * m->_34 + v->w * m->_44);
    *out = r;
    return out;
}

// ---------------- quaternion ----------------

inline D3DXQUATERNION* D3DXQuaternionIdentity(D3DXQUATERNION* out) {
    out->x = 0; out->y = 0; out->z = 0; out->w = 1;
    return out;
}

// Forma LH row-major do D3DX (sinais de w invertidos vs. a forma RH col-major comum)
inline D3DXMATRIX* D3DXMatrixRotationQuaternion(D3DXMATRIX* out, const D3DXQUATERNION* q) {
    float xx = q->x * q->x, yy = q->y * q->y, zz = q->z * q->z;
    float xy = q->x * q->y, xz = q->x * q->z, yz = q->y * q->z;
    float wx = q->w * q->x, wy = q->w * q->y, wz = q->w * q->z;
    out->_11 = 1 - 2 * (yy + zz); out->_12 = 2 * (xy + wz);     out->_13 = 2 * (xz - wy);     out->_14 = 0;
    out->_21 = 2 * (xy - wz);     out->_22 = 1 - 2 * (xx + zz); out->_23 = 2 * (yz + wx);     out->_24 = 0;
    out->_31 = 2 * (xz + wy);     out->_32 = 2 * (yz - wx);     out->_33 = 1 - 2 * (xx + yy); out->_34 = 0;
    out->_41 = 0; out->_42 = 0; out->_43 = 0; out->_44 = 1;
    return out;
}

// Slerp do D3DX: flip por dot<0, fallback linear perto de 1.
// O threshold exato e a (falta de) normalização do resultado saem do golden test —
// se divergir, ajustar AQUI e nunca nos call-sites.
inline D3DXQUATERNION* D3DXQuaternionSlerp(D3DXQUATERNION* out, const D3DXQUATERNION* q1,
                                           const D3DXQUATERNION* q2, float t) {
    float dot = q1->x * q2->x + q1->y * q2->y + q1->z * q2->z + q1->w * q2->w;
    D3DXQUATERNION b = *q2;
    if (dot < 0.0f) {
        dot = -dot;
        b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w;
    }
    float s0, s1;
    if (dot > 0.9995f) {
        s0 = 1.0f - t; s1 = t;
    } else {
        float theta = acosf(dot);
        float invSin = 1.0f / sinf(theta);
        s0 = sinf((1.0f - t) * theta) * invSin;
        s1 = sinf(t * theta) * invSin;
    }
    out->x = s0 * q1->x + s1 * b.x;
    out->y = s0 * q1->y + s1 * b.y;
    out->z = s0 * q1->z + s1 * b.z;
    out->w = s0 * q1->w + s1 * b.w;
    return out;
}

// ---------------- cor ----------------

inline D3DXCOLOR* D3DXColorModulate(D3DXCOLOR* out, const D3DXCOLOR* a, const D3DXCOLOR* b) {
    out->r = a->r * b->r; out->g = a->g * b->g; out->b = a->b * b->b; out->a = a->a * b->a;
    return out;
}

inline D3DXCOLOR* D3DXColorLerp(D3DXCOLOR* out, const D3DXCOLOR* a, const D3DXCOLOR* b, float t) {
    out->r = a->r + t * (b->r - a->r);
    out->g = a->g + t * (b->g - a->g);
    out->b = a->b + t * (b->b - a->b);
    out->a = a->a + t * (b->a - a->a);
    return out;
}

// ---------------- implementações maiores (TMMath.cpp) ----------------

D3DXMATRIX* D3DXMatrixInverse(D3DXMATRIX* out, float* determinant, const D3DXMATRIX* in);
D3DXQUATERNION* D3DXQuaternionRotationMatrix(D3DXQUATERNION* out, const D3DXMATRIX* m);
D3DXVECTOR3* D3DXVec3Project(D3DXVECTOR3* out, const D3DXVECTOR3* v, const D3DVIEWPORT9* vp,
                             const D3DXMATRIX* proj, const D3DXMATRIX* view, const D3DXMATRIX* world);
BOOL D3DXIntersectTri(const D3DXVECTOR3* p0, const D3DXVECTOR3* p1, const D3DXVECTOR3* p2,
                      const D3DXVECTOR3* rayPos, const D3DXVECTOR3* rayDir,
                      float* u, float* v, float* dist);
