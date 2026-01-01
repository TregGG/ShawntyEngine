#include "openglclass.h"

#include "../core/system.h"
#include<glad/glad.h>
#include<iostream>


 OpenGLClass::OpenGLClass()
{

}

 OpenGLClass::~OpenGLClass()
{

}

bool OpenGLClass::Initialize(System* system, int screenWidth, int screenHeight)
{
    if(!system)
    {
        std::cerr<<"OpenGLClass::Initialize failed: System is null\n";
        return false;
    }

    m_system=system;
    m_screenHeight=screenHeight;
    m_screenWidth=screenWidth;

    glViewport(0,0,m_screenWidth,m_screenHeight);

    glEnable(GL_DEPTH_TEST);

    glClearColor(0.1f,0.1f,0.2f,0.1f);

    std::cout<<"OpenGLClass::Initialize: passed";
    return true;

}

void OpenGLClass::BeginFrame()
{
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

void OpenGLClass::EndFrame()
{
    m_system->SwapBuffers();
}

void OpenGLClass::Shutdown()
{
    m_system = nullptr;

}

