#ifndef SANITYCHECK_H_INCLUDED
#define SANITYCHECK_H_INCLUDED

#pragma once
#include "core/game.h"

#include <glm/glm.hpp>
#include "render/spriterendererclass.h"

class SanityGame : public Game
{
public:
    bool OnInit() override;
    void OnInput(const Input& input) override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnShutdown() override;




private:
    bool m_RendererReady = false;

    glm::vec2 m_PlayerPos { 0.0f, 0.0f };   // WASD-controlled quad
    glm::vec2 m_StaticPos { 3.0f, 0.0f };   // reference quad
    SpriteRendererClass m_SpriteRenderer;

    TextureGPU m_TestTexture{};
    SpriteFrame m_TestFrame{};

    glm::mat4 m_Projection{};
    float m_MoveSpeed = 5.0f;               // world units / second
};

#endif // SANITYCHECK_H_INCLUDED
