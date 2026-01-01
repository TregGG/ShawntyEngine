#pragma once

#include <array>
#include <vector>
#include "system.h"   // for RawInputEvent

class Input
{
public:

    static constexpr int MAX_KEYS = 512;
    static constexpr int MAX_MOUSE_BUTTONS = 8;

    Input();

    // Frame lifecycle
    void BeginFrame();
    void ProcessEvents(const std::vector<RawInputEvent>& events);

    // Queries
    bool IsKeyDown(int key) const;
    bool IsKeyPressed(int key) const;
    bool IsKeyReleased(int key) const;
    bool IsMouseButtonDown(int button) const;
    bool IsMouseButtonPressed(int button) const;
    bool IsMouseButtonReleased(int button) const;
    void GetMousePosition(double& x, double& y) const;
    void GetMouseDelta(double& dx, double& dy) const;

private:

    std::array<bool, MAX_MOUSE_BUTTONS> m_CurrentMouseButtons{};
    std::array<bool, MAX_MOUSE_BUTTONS> m_PreviousMouseButtons{};

    double m_MouseX = 0.0;
    double m_MouseY = 0.0;
    double m_PrevMouseX = 0.0;
    double m_PrevMouseY = 0.0;

    std::array<bool, MAX_KEYS> m_CurrentKeys{};
    std::array<bool, MAX_KEYS> m_PreviousKeys{};
};
