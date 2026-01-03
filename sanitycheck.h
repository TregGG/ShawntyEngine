#ifndef SANITYCHECK_H_INCLUDED
#define SANITYCHECK_H_INCLUDED

#pragma once
#include "core/game.h"

#include <glm/glm.hpp>

class SanityGame : public Game
{
public:
    bool OnInit() override;
    void OnInput(const Input& input) override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnShutdown() override;




private:
    glm::vec2 m_PlayerPos { 0.0f, 0.0f };   // WASD-controlled quad
    glm::vec2 m_StaticPos { 3.0f, 0.0f };   // reference quad

    float m_MoveSpeed = 5.0f;               // world units / second
};

#endif // SANITYCHECK_H_INCLUDED
