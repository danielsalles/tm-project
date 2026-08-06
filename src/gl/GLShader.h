#pragma once

#include <glad/gl.h>
#include <string>

namespace tmx {

// Compiled+linked GLSL program. Source is assembled as:
//   "#version 410 core\n" + common.glsl + body
// so every shader sees FIX_Z and the FrameData UBO block.
class GLShader {
public:
    GLShader() = default;
    ~GLShader();

    GLShader(const GLShader&) = delete;
    GLShader& operator=(const GLShader&) = delete;

    bool Build(const char* commonSrc, const char* vertBody, const char* fragBody,
               std::string* outLog);
    void Destroy();

    void Bind() const;

    GLint UniformLoc(const char* name) const { return glGetUniformLocation(m_program, name); }
    GLuint Program() const { return m_program; }

    // Upload helper implementing the project convention (04 §4.3): a row-major
    // D3DXMATRIX in memory is already the column-major transpose GLSL expects.
    void SetMat4(GLint loc, const float* rowMajor16) const {
        glUniformMatrix4fv(loc, 1, GL_FALSE, rowMajor16);
    }

private:
    GLuint m_program = 0;
};

}
