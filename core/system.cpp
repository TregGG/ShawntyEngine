#include"system.h"
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#define ENGINE_CLASS "System"
#include "enginedebug.h"


//STATICS->InputMANAGEMENT

static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    System* system = static_cast<System*>(glfwGetWindowUserPointer(window));
    if (!system)
        return;

    RawInputEvent event{};
    event.type = InputEventType::Key;
    event.key = key;
    event.action = action;

    system->PushRawInputEvent(event);
}


static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    System* system = static_cast<System*>(glfwGetWindowUserPointer(window));
    if (!system)
        return;

    RawInputEvent event{};
    event.type = InputEventType::MouseButton;
    event.key = button;
    event.action = action;

    system->PushRawInputEvent(event);
}

static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    System* system = static_cast<System*>(glfwGetWindowUserPointer(window));
    if (!system)
        return;

    RawInputEvent event{};
    event.type = InputEventType::MouseMove;
    event.mouseX = xpos;
    event.mouseY = ypos;

    system->PushRawInputEvent(event);
}

 System::System()
{

}

System::~System()
{

}

bool System::Initialize(int width, int height, const char* title)
{
    ENGINE_LOG("Initializing window %dx%d", width, height);
    m_shouldClose=false;
    if(!glfwInit())
    {
        ENGINE_ERROR("Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
//    glfwWindowHint(GLFW_VERSION_MAJOR,3);
//    glfwWindowHint(GLFW_VERSION_MINOR,3);
//    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);


    m_Window= glfwCreateWindow(width,height,title,nullptr,nullptr);
    if(!m_Window)
    {
        ENGINE_ERROR("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_Window);
    glfwSetWindowUserPointer(m_Window,this);

    //setting input callbacks
    glfwSetKeyCallback(m_Window,KeyCallback);
    glfwSetMouseButtonCallback(m_Window,MouseButtonCallback);
    glfwSetCursorPosCallback(m_Window,CursorPosCallback);


    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        ENGINE_ERROR("Failed to initialize GLAD");
        return false;
    }
    glfwSwapInterval(1);                            //VSYNC ON
    ENGINE_LOG("OpenGL Vendor: %s", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    ENGINE_LOG("OpenGL Renderer: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    ENGINE_LOG("OpenGL Version: %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    ENGINE_LOG("Window initialized");
    glfwSetWindowUserPointer(m_Window, this);

    glfwSetFramebufferSizeCallback(
        m_Window,
        [](GLFWwindow* window, int width, int height)
        {
            auto* system = static_cast<System*>(glfwGetWindowUserPointer(window));
            system->OnWindowResized(width, height);
        }
    );
    
    return true;
}

void System::PollEvents()
{

   glfwPollEvents();

    if (m_Window && glfwWindowShouldClose(m_Window))
        {m_shouldClose = true;}
     //std::this_thread::sleep_for(std::chrono::milliseconds(16));
     //glfwWaitEventsTimeout(0.016);

    //TODO: add events
}

bool System::ShouldClose() const
{
    return m_shouldClose;
}

void System::Shutdown()
{
     if (m_Window)
    {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }

    glfwTerminate();
    //TODO:
}

void System::SwapBuffers()
{
    glfwSwapBuffers(m_Window);
}


//  ----    INPUT MANAGEMENT      ----
const std::vector<RawInputEvent>& System::GetInputEvents() const
{
    return m_InputEvents;
}
void System::ClearInputEvents()
{
    m_InputEvents.clear();
}


std::vector<RawInputEvent>& System::GetInputEventsInternal()
{
    return m_InputEvents;
}

void System::PushRawInputEvent(const RawInputEvent& event)
{
    m_InputEvents.push_back(event);
}


void System::OnWindowResized(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    m_Width = width;
    m_Height = height;
    ENGINE_LOG("Window resized to %dx%d", width, height);

    glViewport(0, 0, width, height);

    // DO NOT touch scene or camera here
}
