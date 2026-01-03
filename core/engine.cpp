#include"engine.h"

#include<glad/glad.h>
#include "game.h"
#include "system.h"
#include "timer.h"
#include "input.h"

#include "../render/openglclass.h"
#include "../render/camera.h"


Engine::Engine()
{
    m_Running=true;

}

Engine::~Engine()
{

}

bool Engine::Initialize(Game* game)
{
    if(!game)
    {
        return false;
    }
    m_Game=game;
    m_System= new System();
    m_Timer= new Timer();
    m_Input= new Input();
    m_OpenGL=new OpenGLClass();
    m_Camera=new Camera(20,16);// just temp, ideally we want scene to create the camera

    if(!m_System->Initialize(800,600,"Engine"))
    {
        return false;
    }
    if (!m_OpenGL->Initialize(m_System, 800, 600))
    {
         return false;
    }
    m_Timer->Start();
    if(!m_Game->OnInit())
    {
        return false;
    }
    m_OpenGL->SetCamera(m_Camera);
    m_Game->SetCamera(m_Camera);
    m_Game->SetRenderer(m_OpenGL);
    return true;
}

void Engine::Run()
{
    m_Running = true;

    while(m_Running)
    {
            m_System->PollEvents();

        if (m_System->ShouldClose())
        {
            Quit();
            break;
        }

        // Time & input
        m_Timer->Tick();
        m_Input->BeginFrame();
        m_Input->ProcessEvents(m_System->GetInputEvents());
        m_System->ClearInputEvents();

        m_Game->OnInput(*m_Input);
        m_Game->OnUpdate(m_Timer->GetDeltaTime());

        // 🔹 Rendering phase
        m_OpenGL->BeginFrame();
        m_Game->OnRender();
        m_OpenGL->EndFrame();

        }
}
void Engine::Shutdown()
{
    if(m_Game)
    {
        m_Game->OnShutdown();
        m_Game=nullptr;
    }
    if (m_Input)
    {
        delete m_Input;
        m_Input = nullptr;
    }

    if (m_OpenGL)
    {
        m_OpenGL->Shutdown();
        delete m_OpenGL;
        m_OpenGL = nullptr;
    }

    if (m_Timer)
    {
        delete m_Timer;
        m_Timer = nullptr;
    }

    if (m_System)
    {
        m_System->Shutdown();
        delete m_System;
        m_System = nullptr;
    }
}

void Engine::Quit()
{
    m_Running=false;
}
