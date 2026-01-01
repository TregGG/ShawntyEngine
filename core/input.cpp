#include "input.h"
#include <GLFW/glfw3.h>

Input::Input()
{
    m_CurrentKeys.fill(false);
    m_PreviousKeys.fill(false);

    m_CurrentMouseButtons.fill(false);
    m_PreviousMouseButtons.fill(false);
}

void Input::BeginFrame()
{
    m_PreviousKeys = m_CurrentKeys;
    m_PreviousMouseButtons = m_CurrentMouseButtons;

    m_PrevMouseX = m_MouseX;
    m_PrevMouseY = m_MouseY;
}

void Input::ProcessEvents(const std::vector<RawInputEvent>& events)
{
    for (const RawInputEvent& event : events)
    {
        switch (event.type)
        {
            case InputEventType::Key:
            {
                if (event.key < 0 || event.key >= MAX_KEYS)
                    break;

                if (event.action == GLFW_PRESS)
                    m_CurrentKeys[event.key] = true;
                else if (event.action == GLFW_RELEASE)
                    m_CurrentKeys[event.key] = false;
                break;
            }

            case InputEventType::MouseButton:
            {
                if (event.key < 0 || event.key >= MAX_MOUSE_BUTTONS)
                    break;

                if (event.action == GLFW_PRESS)
                    m_CurrentMouseButtons[event.key] = true;
                else if (event.action == GLFW_RELEASE)
                    m_CurrentMouseButtons[event.key] = false;
                break;
            }

            case InputEventType::MouseMove:
            {
                m_MouseX = event.mouseX;
                m_MouseY = event.mouseY;
                break;
            }
        }
    }
}


bool Input::IsKeyDown(int key) const
{
    if (key < 0 || key >= MAX_KEYS)
        return false;

    return m_CurrentKeys[key];
}

bool Input::IsKeyPressed(int key) const
{
    if (key < 0 || key >= MAX_KEYS)
        return false;

    return m_CurrentKeys[key] && !m_PreviousKeys[key];
}

bool Input::IsKeyReleased(int key) const
{
    if (key < 0 || key >= MAX_KEYS)
        return false;

    return !m_CurrentKeys[key] && m_PreviousKeys[key];
}
bool Input::IsMouseButtonDown(int button) const
{
    if (button < 0 || button >= MAX_MOUSE_BUTTONS)
        return false;

    return m_CurrentMouseButtons[button];
}

bool Input::IsMouseButtonPressed(int button) const
{
    if (button < 0 || button >= MAX_MOUSE_BUTTONS)
        return false;

    return m_CurrentMouseButtons[button] &&
          !m_PreviousMouseButtons[button];
}

bool Input::IsMouseButtonReleased(int button) const
{
    if (button < 0 || button >= MAX_MOUSE_BUTTONS)
        return false;

    return !m_CurrentMouseButtons[button] &&
            m_PreviousMouseButtons[button];
}

void Input::GetMousePosition(double& x, double& y) const
{
    x = m_MouseX;
    y = m_MouseY;
}

void Input::GetMouseDelta(double& dx, double& dy) const
{
    dx = m_MouseX - m_PrevMouseX;
    dy = m_MouseY - m_PrevMouseY;
}

