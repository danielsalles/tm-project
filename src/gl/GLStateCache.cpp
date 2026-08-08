#include "gl/GLStateCache.h"

namespace tmx {

void GLStateCache::Invalidate() {
    m_valid = false;
    scissorEnabled = false;
}

void GLStateCache::SetScissor(int x, int y, int w, int h) {
    if (!scissorEnabled) {
        glEnable(GL_SCISSOR_TEST);
        scissorEnabled = true;
    }
    if (scissorX != x || scissorY != y || scissorW != w || scissorH != h) {
        glScissor(x, y, w, h);
        scissorX = x; scissorY = y; scissorW = w; scissorH = h;
    }
}

void GLStateCache::DisableScissor() {
    if (scissorEnabled) {
        glDisable(GL_SCISSOR_TEST);
        scissorEnabled = false;
    }
}

void GLStateCache::Apply() {
    if (!m_valid) {
        c_depthTest = !depthTest;
        c_depthWrite = !depthWrite;
        c_blend = !blend;
        c_cull = !cull;
        c_depthFunc = 0;
        c_cullFaceMode = 0;
        c_blendSrc = 0;
        c_blendDst = 0;
        c_texture[0] = c_texture[1] = 0xFFFFFFFF;
        c_sampler[0] = c_sampler[1] = 0xFFFFFFFF;
        m_valid = true;
    }

    if (c_depthTest != depthTest) {
        c_depthTest = depthTest;
        if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    }
    if (c_depthWrite != depthWrite) {
        c_depthWrite = depthWrite;
        glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);
    }
    if (c_depthFunc != depthFunc) {
        c_depthFunc = depthFunc;
        glDepthFunc(depthFunc);
    }
    if (c_blend != blend) {
        c_blend = blend;
        if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    }
    if (c_blendSrc != blendSrc || c_blendDst != blendDst) {
        c_blendSrc = blendSrc;
        c_blendDst = blendDst;
        glBlendFunc(blendSrc, blendDst);
    }
    if (c_cull != cull) {
        c_cull = cull;
        if (cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    }
    if (c_cullFaceMode != cullFaceMode) {
        c_cullFaceMode = cullFaceMode;
        glCullFace(cullFaceMode);
    }
    for (int i = 0; i < 2; ++i) {
        if (c_texture[i] != texture[i] || c_sampler[i] != sampler[i]) {
            c_texture[i] = texture[i];
            c_sampler[i] = sampler[i];
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, texture[i]);
            glBindSampler(i, sampler[i]);
        }
    }
    glActiveTexture(GL_TEXTURE0);
}

}
