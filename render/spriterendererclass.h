#pragma once

#include <glm/glm.hpp>
#include "../assets/assetdatastruct.h"


class SpriteRendererClass
{
public:
    bool Initialize();
    void Shutdown();

    void DrawSprite(const TextureGPU& texture,
                    const SpriteFrame& frame,
                    const glm::mat4& mvp);

private:
    void SetupQuad();
    void SetupShader();

private:
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_EBO = 0;
    unsigned int m_Shader = 0;

    bool m_Initialized = false;

};
