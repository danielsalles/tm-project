#pragma once

#include <glad/gl.h>
#include <string>

#include "gl/GLMesh.h"
#include "gl/GLShader.h"
#include "math/TMMath.h"

namespace tmx {

class GLRenderDevice;
class GLTextureManager;

// Sky dome + weather state (port of TMSky minus the billboard sun/moon/stars,
// which are phase-4 effects). The dome is mesh\sky001.msa (MeshList index 1);
// the weather picks its texture (model list 67+state) and drives fog, clear
// color and the two directional light colors (TMSky::SetWeatherState /
// FrameMove + RenderDevice block 1).
class SkyDome {
public:
    bool Init(const std::string& meshListTxt, GLTextureManager& textures,
              std::string* err);
    void Destroy();

    // state 0=clear, 1=cloudy, 2=rain, 3=snow (day states; +10 = night, TODO D4+)
    void SetWeather(int state);
    int  Weather() const { return m_weather; }

    const float* ClearColor() const { return m_clear; }   // rgb 0..1

    // Applies fog + light colors for the current weather to the device.
    void ApplyWeather(GLRenderDevice& device) const;

    // Draws the dome centered on the camera (depth test on, depth write off).
    void Render(GLRenderDevice& device, float camX, float camZ);

private:
    GLShader  m_shader;
    GLint     m_locWorld = -1;
    GLint     m_locTex0 = -1;
    GLMesh    m_dome;
    bool      m_hasDome = false;
    GLTextureManager* m_textures = nullptr;
    int       m_weather = 0;
    float     m_clear[3] = { 0.11f, 0.47f, 0.74f };
};

}
