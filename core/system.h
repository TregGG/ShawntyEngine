//connection of os to our engine
#pragma once
#include<vector>
struct GLFWwindow;

enum class InputEventType
{
    Key,
    MouseButton,
    MouseMove
};
struct RawInputEvent
{
    InputEventType type;
    int key;
    int action;

    double mouseX;
    double mouseY;
};

class System
{
public:
    System();
    ~System();

    bool Initialize(int width,int height,const char* title);

    void PollEvents();

    bool ShouldClose() const;

    void Shutdown();

    void SwapBuffers();
     void OnWindowResized(int width, int height);

    int GetWindowWidth() const { return m_Width; }
    int GetWindowHeight() const { return m_Height; }

    //Input Management
    const std::vector<RawInputEvent>& GetInputEvents() const;
    void ClearInputEvents();
    void PushRawInputEvent(const RawInputEvent& event);

protected:
    //for the internal static functions
    std::vector<RawInputEvent>& GetInputEventsInternal();
private:

    GLFWwindow* m_Window=nullptr;
    int m_Width = 1280;
    int m_Height = 720;
    bool m_shouldClose=false;
    std::vector<RawInputEvent> m_InputEvents;
};
