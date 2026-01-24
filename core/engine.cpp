#include"engine.h"

#include<glad/glad.h>
#include "game.h"
#include "system.h"
#include "timer.h"
#include "input.h"

#include "../render/openglclass.h"


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

        // Rendering phase - delegate to Game
        m_Game->OnRender();
        
        // Swap buffers
        m_System->SwapBuffers();
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
