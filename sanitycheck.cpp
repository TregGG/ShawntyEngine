#include "sanitycheck.h"
#include "core/input.h"
#include"render/camera.h"
#include"render/openglclass.h"
#include <iostream>
#include <GLFW/glfw3.h>
static glm::vec2 g_MoveDir { 0.0f, 0.0f };
static glm::vec2 g_CameraDir { 0.0f, 0.0f };


bool SanityGame::OnInit()
{
    std::cout << "[SanityGame] Initialized\n";
    std::cout << "Press WASD, mouse buttons, move mouse\n";
    return true;
}

void SanityGame::OnInput(const Input& input)
{
    g_MoveDir = { 0.0f, 0.0f };
    g_CameraDir = { 0.0f, 0.0f };

    // ---- WASD → move quad ----
    if (input.IsKeyDown(GLFW_KEY_W)) g_MoveDir.y += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_S)) g_MoveDir.y -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_A)) g_MoveDir.x -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_D)) g_MoveDir.x += 1.0f;

    // ---- Arrow keys → move camera ----
    if (input.IsKeyDown(GLFW_KEY_UP))    g_CameraDir.y += 1.0f;
    if (input.IsKeyDown(GLFW_KEY_DOWN))  g_CameraDir.y -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_LEFT))  g_CameraDir.x -= 1.0f;
    if (input.IsKeyDown(GLFW_KEY_RIGHT)) g_CameraDir.x += 1.0f;
}

void SanityGame::OnUpdate(float deltaTime)
{
    // Move quad
    if (g_MoveDir.x != 0.0f || g_MoveDir.y != 0.0f)
    {
        m_PlayerPos += g_MoveDir * m_MoveSpeed * deltaTime;
    }

    // Move camera
    if (g_CameraDir.x != 0.0f || g_CameraDir.y != 0.0f)
    {
        glm::vec2 camPos = m_Camera->GetCameraPosition();
        camPos += g_CameraDir * m_MoveSpeed * deltaTime;
        m_Camera->SetCameraPosition(camPos.x, camPos.y);
    }
}


void SanityGame::OnRender()
{
    // Movable quad (player)
    m_Renderer->DrawQuad(m_PlayerPos);

    // Static quad (reference)
    m_Renderer->DrawQuad(m_StaticPos);
}


void SanityGame::OnShutdown()
{
    std::cout << "[SanityGame] Shutdown\n";
}
