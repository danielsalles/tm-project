#include "gl/GLShader.h"

#include <cstdio>
#include <cstring>

namespace tmx {

static GLuint CompileOne(GLenum type, const char* commonSrc, const char* body,
                         std::string* outLog) {
    const char* parts[] = {
        "#version 410 core\n",
        commonSrc,
        body,
    };
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 3, parts, nullptr);
    glCompileShader(sh);

    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok && outLog) {
        char buf[4096];
        glGetShaderInfoLog(sh, sizeof buf, nullptr, buf);
        *outLog += buf;
    }
    if (!ok) {
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

GLShader::~GLShader() {
    Destroy();
}

void GLShader::Destroy() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

bool GLShader::Build(const char* commonSrc, const char* vertBody, const char* fragBody,
                     std::string* outLog) {
    Destroy();

    GLuint vs = CompileOne(GL_VERTEX_SHADER, commonSrc, vertBody, outLog);
    GLuint fs = CompileOne(GL_FRAGMENT_SHADER, commonSrc, fragBody, outLog);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        if (outLog) {
            char buf[4096];
            glGetProgramInfoLog(m_program, sizeof buf, nullptr, buf);
            *outLog += buf;
        }
        Destroy();
        return false;
    }

    GLuint blockIdx = glGetUniformBlockIndex(m_program, "FrameData");
    if (blockIdx != GL_INVALID_INDEX)
        glUniformBlockBinding(m_program, blockIdx, 0);

    return true;
}

void GLShader::Bind() const {
    glUseProgram(m_program);
}

}
