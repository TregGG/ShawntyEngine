#include "sanitycheck.h"
#include "core/input.h"

#include <iostream>
#include <GLFW/glfw3.h>

bool SanityGame::OnInit()
{
    std::cout << "[SanityGame] Initialized\n";
    std::cout << "Press WASD, mouse buttons, move mouse\n";
    return true;
}

void SanityGame::OnInput(const Input& input)
{
    // ---- Keyboard sanity ----
    if (input.IsKeyPressed(GLFW_KEY_W))
        std::cout << "W pressed\n";

    if (input.IsKeyReleased(GLFW_KEY_W))
        std::cout << "W released\n";

    if (input.IsKeyDown(GLFW_KEY_A))
        std::cout << "Holding A\n";

    // ---- Mouse buttons ----
    if (input.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT))
        std::cout << "Left mouse button pressed\n";

    if (input.IsMouseButtonReleased(GLFW_MOUSE_BUTTON_LEFT))
        std::cout << "Left mouse button released\n";

    // ---- Mouse position ----
    double x, y;
    input.GetMousePosition(x, y);
    //std::cout << "Mouse Pos: (" << x << ", " << y << ")\n";

    // ---- Mouse delta ----
    double dx, dy;
    input.GetMouseDelta(dx, dy);
    if (dx != 0.0 || dy != 0.0)
    {
      //  std::cout << "Mouse Delta: (" << dx << ", " << dy << ")\n";
    }
}

void SanityGame::OnUpdate(float deltaTime)
{
    // Optional: show delta time occasionally
    // std::cout << "Delta: " << deltaTime << "\n";
}

void SanityGame::OnRender()
{
    // Nothing to render yet
}

void SanityGame::OnShutdown()
{
    std::cout << "[SanityGame] Shutdown\n";
}
