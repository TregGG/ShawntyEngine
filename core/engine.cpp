#include"engine.h"

#include<glad/glad.h>
#include <thread>
#include <chrono>
#include "game.h"
#include "system.h"
#include "timer.h"
#include "input.h"
#include "logger.h"
#include "../render/openglclass.h"
#include "../services/base/eventservice.h"
#include "../services/networkservice.h"
#include "../services/networkcontrol.h"
#include "engineconfig.h"
#define ENGINE_CLASS "Engine"
#include "enginedebug.h"

Engine::Engine()
{
    m_Running=true;

}

Engine::~Engine()
{

}

bool Engine::Initialize(Game* game, bool isServer)
{
    m_IsServer = isServer;
    #if defined(ENGINE_RELEASE)

        // No logging in release

    #elif defined(ENGINE_LOG_CONSOLE)

        Logger::Init(Logger::Output::Console);

    #elif defined(ENGINE_LOG_FILE)

        Logger::Init(Logger::Output::File);

    #else

        // Default
        Logger::Init(Logger::Output::Both);

    #endif
    if(!game)
    {
        ENGINE_ERROR("Initialize failed: game is null");
        return false;
    }
    m_Game=game;
    m_System= new System();
    m_Timer= new Timer();
    m_Input= new Input();
    m_EventService = new EventService();
    m_NetworkService = new NetworkService();
    m_NetworkControl = m_Game->CreateNetworkControl();
    if (!m_NetworkControl) {
        m_NetworkControl = new NetworkControl();
    }
    m_OpenGL=new OpenGLClass();

    if(!m_System->Initialize(800,600,"Engine", m_IsServer))
    {
           ENGINE_ERROR("Initialize failed: System::Initialize failed");
        return false;
    }
    
    if (!m_IsServer) {
        if (!m_OpenGL->Initialize(m_System, 800, 600))
        {
                ENGINE_ERROR("Initialize failed: OpenGLClass::Initialize failed");
             return false;
        }
    }
    
    m_EventService->Init();
    
    m_NetworkService->Init();
    m_NetworkControl->Init();
    m_NetworkControl->SetNetworkService(m_NetworkService);
    
    m_NetworkService->OnShutdownRequested = [this]() {
        this->Quit();
    };
    
    if (m_IsServer) {
        if (!m_NetworkService->Host(7777)) {
            ENGINE_ERROR("Failed to start server on port 7777. ENet Host creation failed. (Port probably in use).");
            return false;
        }
    }
    
    m_Game->SetEventService(m_EventService);
    m_Game->SetNetworkServices(m_NetworkService, m_NetworkControl);
    m_Game->SetServerMode(m_IsServer);
    m_Timer->Start();
    
    if(!m_Game->OnInit())
    {
        ENGINE_ERROR("Initialize failed: Game::OnInit failed");
        return false;
    }
    ENGINE_LOG("Engine initialized");


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
        int w = m_System->GetWindowWidth();
        int h = m_System->GetWindowHeight();

        if (w != m_LastWidth || h != m_LastHeight)
        {
            m_LastWidth = w;
            m_LastHeight = h;

            OnWindowResized(w, h);
        }

        // Time & input
        m_Timer->Tick();
        
        if (!m_IsServer) {
            m_Input->BeginFrame();
            m_Input->ProcessEvents(m_System->GetInputEvents(), m_EventService);
            m_System->ClearInputEvents();
            m_Game->OnInput(*m_Input);
        }

        m_NetworkService->Update(m_Timer->GetDeltaTime());
        m_NetworkControl->Update(m_Timer->GetDeltaTime());

        m_Game->OnUpdate(m_Timer->GetDeltaTime());

        if (!m_IsServer) {
            // Rendering phase - delegate to Game
            m_Game->OnRender();
            
            // Swap buffers
            m_System->SwapBuffers();
        } else {
            // Cap server tick rate to ~60 Hz to avoid CPU core pegging and network packet flooding
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        }
}
void Engine::Shutdown()
{
    #if !defined(ENGINE_RELEASE)
        Logger::Shutdown();
    #endif
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

    if (m_EventService)
    {
        m_EventService->Shutdown();
        delete m_EventService;
        m_EventService = nullptr;
    }

    if (m_NetworkControl)
    {
        m_NetworkControl->Shutdown();
        delete m_NetworkControl;
        m_NetworkControl = nullptr;
    }

    if (m_NetworkService)
    {
        m_NetworkService->Shutdown();
        delete m_NetworkService;
        m_NetworkService = nullptr;
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

    ENGINE_LOG("Engine shutdown complete");
}

void Engine::Quit()
{
    m_Running=false;
}

void Engine::OnWindowResized(int width, int height)
{
    if (m_Game)
    {
        m_Game->OnResize(width, height);
    }
}
