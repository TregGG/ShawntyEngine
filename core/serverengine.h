#ifndef SERVERENGINE_H
#define SERVERENGINE_H

#pragma once

class ServerGame;
class System;
class Timer;
class EventService;
class NetworkService;
class NetworkControl;

class ServerEngine
{
public:
    ServerEngine();
    ~ServerEngine();

    bool Initialize(ServerGame* game);
    void Run();
    void Shutdown();

    void Quit();

private:
    bool m_Running = false;

    System* m_System = nullptr;
    Timer* m_Timer = nullptr;
    EventService* m_EventService = nullptr;
    NetworkService* m_NetworkService = nullptr;
    NetworkControl* m_NetworkControl = nullptr;
    
    // Not owned by engine
    ServerGame* m_Game = nullptr;
};

#endif // SERVERENGINE_H
