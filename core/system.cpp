#include"system.h"
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<iostream>
#include<thread>
#include<chrono>
#include <iostream>


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
    std::cout<<"Initializing window\n";
    m_shouldClose=false;
    if(!glfwInit())
    {
        std::cerr<<"Failed to initialize GLFW\n";
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
        std::cerr<<"Failed to create a GLFW window\n";
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
        std::cerr<<"Failed to initialize GLAD\n";
    }
    glfwSwapInterval(1);                            //VSYNC ON
    std::cout << "OpenGL Vendor:   " << glGetString(GL_VENDOR) << "\n";
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "OpenGL Version:  " << glGetString(GL_VERSION) << "\n";

    std::cout<<"Initialized window\n";
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
    std::cout << "System: Window resized to " << width << "x" << height << "\n";

    glViewport(0, 0, width, height);

    // DO NOT touch scene or camera here
}
