#include "serverengine.h"

#include <thread>
#include <chrono>
#include "servergame.h"
#include "system.h"
#include "timer.h"
#include "logger.h"
#include "../services/base/eventservice.h"
#include "../services/networkservice.h"
#include "../services/networkcontrol.h"

#define ENGINE_CLASS "ServerEngine"
#include "enginedebug.h"

ServerEngine::ServerEngine()
{
    m_Running = true;
}

ServerEngine::~ServerEngine()
{
}

bool ServerEngine::Initialize(ServerGame* game)
{
#if defined(ENGINE_RELEASE)
    // No logging in release
#elif defined(ENGINE_LOG_CONSOLE)
    Logger::Init(Logger::Output::Console);
#elif defined(ENGINE_LOG_FILE)
    Logger::Init(Logger::Output::File);
#else
    Logger::Init(Logger::Output::Both);
#endif

    if (!game)
    {
        ENGINE_ERROR("Initialize failed: game is null");
        return false;
    }

    m_Game = game;
    m_System = new System();
    m_Timer = new Timer();
    m_EventService = new EventService();
    m_NetworkService = new NetworkService();
    m_NetworkControl = m_Game->CreateNetworkControl();
    if (!m_NetworkControl) {
        m_NetworkControl = new NetworkControl();
    }

    // Initialize System in headless mode
    if (!m_System->Initialize(800, 600, "ServerEngine", true))
    {
        ENGINE_ERROR("Initialize failed: System::Initialize headless failed");
        return false;
    }

    m_EventService->Init();
    
    m_NetworkService->Init();
    m_NetworkControl->Init();
    m_NetworkControl->SetNetworkService(m_NetworkService);
    
    m_NetworkService->OnShutdownRequested = [this]() {
        this->Quit();
    };
    
    if (!m_NetworkService->Host(7777)) {
        ENGINE_ERROR("Failed to start server on port 7777. ENet Host creation failed. (Port probably in use).");
        return false;
    }
    
    m_Game->SetEventService(m_EventService);
    m_Game->SetNetworkServices(m_NetworkService, m_NetworkControl);
    
    m_Timer->Start();
    
    if (!m_Game->OnInit())
    {
        ENGINE_ERROR("Initialize failed: Game::OnInit failed");
        return false;
    }
    
    ENGINE_LOG("ServerEngine initialized");

    return true;
}

void ServerEngine::Run()
{
    m_Running = true;

    while (m_Running)
    {
        m_System->PollEvents();

        if (m_System->ShouldClose())
        {
            Quit();
            break;
        }

        m_Timer->Tick();
        
        m_NetworkService->Update(m_Timer->GetDeltaTime());
        m_NetworkControl->Update(m_Timer->GetDeltaTime());

        m_Game->OnUpdate(m_Timer->GetDeltaTime());

        // Cap server tick rate to ~60 Hz to avoid CPU core pegging and network packet flooding
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void ServerEngine::Shutdown()
{
#if !defined(ENGINE_RELEASE)
    Logger::Shutdown();
#endif
    if (m_Game)
    {
        m_Game->OnShutdown();
        m_Game = nullptr;
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

    ENGINE_LOG("ServerEngine shutdown complete");
}

void ServerEngine::Quit()
{
    m_Running = false;
}
