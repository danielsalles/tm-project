#include "math/TMMath.h"

// Implementações maiores do shim D3DX. Convenções: ver TMMath.h.

D3DXMATRIX* D3DXMatrixInverse(D3DXMATRIX* out, float* determinant, const D3DXMATRIX* in) {
    // Adjugado generalizado 4x4 (mesmo caminho numérico do D3DX: cofatores / det)
    const float* m = &in->m[0][0];
    float inv[16];

    inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

    float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (determinant)
        *determinant = det;
    if (det == 0.0f)
        return nullptr;

    det = 1.0f / det;
    for (int i = 0; i < 16; i++)
        (&out->m[0][0])[i] = inv[i] * det;
    return out;
}

D3DXQUATERNION* D3DXQuaternionRotationMatrix(D3DXQUATERNION* out, const D3DXMATRIX* m) {
    // Shepperd's method sobre a convenção row-major LH (round-trip com
    // D3DXMatrixRotationQuaternion é o requisito — usado no crossfade de animação)
    float trace = m->_11 + m->_22 + m->_33;
    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f);
        out->w = s * 0.5f;
        s = 0.5f / s;
        out->x = (m->_23 - m->_32) * s;
        out->y = (m->_31 - m->_13) * s;
        out->z = (m->_12 - m->_21) * s;
    } else if (m->_11 > m->_22 && m->_11 > m->_33) {
        float s = sqrtf(1.0f + m->_11 - m->_22 - m->_33);
        out->x = s * 0.5f;
        s = 0.5f / s;
        out->y = (m->_12 + m->_21) * s;
        out->z = (m->_31 + m->_13) * s;
        out->w = (m->_23 - m->_32) * s;
    } else if (m->_22 > m->_33) {
        float s = sqrtf(1.0f + m->_22 - m->_11 - m->_33);
        out->y = s * 0.5f;
        s = 0.5f / s;
        out->x = (m->_12 + m->_21) * s;
        out->z = (m->_23 + m->_32) * s;
        out->w = (m->_31 - m->_13) * s;
    } else {
        float s = sqrtf(1.0f + m->_33 - m->_11 - m->_22);
        out->z = s * 0.5f;
        s = 0.5f / s;
        out->x = (m->_31 + m->_13) * s;
        out->y = (m->_23 + m->_32) * s;
        out->w = (m->_12 - m->_21) * s;
    }
    return out;
}

D3DXVECTOR3* D3DXVec3Project(D3DXVECTOR3* out, const D3DXVECTOR3* v, const D3DVIEWPORT9* vp,
                             const D3DXMATRIX* proj, const D3DXMATRIX* view, const D3DXMATRIX* world) {
    D3DXMATRIX m, t;
    D3DXMatrixMultiply(&t, world, view);
    D3DXMatrixMultiply(&m, &t, proj);
    D3DXVECTOR3 p;
    D3DXVec3TransformCoord(&p, v, &m);
    // Viewport D3D: Y cresce para baixo
    out->x = (float)vp->X + (1.0f + p.x) * (float)vp->Width * 0.5f;
    out->y = (float)vp->Y + (1.0f - p.y) * (float)vp->Height * 0.5f;
    out->z = vp->MinZ + p.z * (vp->MaxZ - vp->MinZ);
    return out;
}

BOOL D3DXIntersectTri(const D3DXVECTOR3* p0, const D3DXVECTOR3* p1, const D3DXVECTOR3* p2,
                      const D3DXVECTOR3* rayPos, const D3DXVECTOR3* rayDir,
                      float* u, float* v, float* dist) {
    // Möller–Trumbore, sem culling de backface (comportamento do D3DX9).
    // dist é em unidades do vetor direção (rayDir não precisa ser normalizado).
    D3DXVECTOR3 e1 = *p1 - *p0;
    D3DXVECTOR3 e2 = *p2 - *p0;

    D3DXVECTOR3 p;
    D3DXVec3Cross(&p, rayDir, &e2);
    float det = D3DXVec3Dot(&e1, &p);
    if (fabsf(det) < 1e-6f)
        return FALSE;

    float invDet = 1.0f / det;
    D3DXVECTOR3 tvec = *rayPos - *p0;
    float uu = D3DXVec3Dot(&tvec, &p) * invDet;
    if (uu < 0.0f || uu > 1.0f)
        return FALSE;

    D3DXVECTOR3 q;
    D3DXVec3Cross(&q, &tvec, &e1);
    float vv = D3DXVec3Dot(rayDir, &q) * invDet;
    if (vv < 0.0f || uu + vv > 1.0f)
        return FALSE;

    float t = D3DXVec3Dot(&e2, &q) * invDet;
    if (t < 0.0f)
        return FALSE;

    if (u) *u = uu;
    if (v) *v = vv;
    if (dist) *dist = t;
    return TRUE;
}
