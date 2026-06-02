#ifndef SERVERGAME_H
#define SERVERGAME_H

#pragma once

#include "../levels/scenemanager.h"
#include "../assets/assetmanager.h"

class EventService;
class NetworkService;
class NetworkControl;

class ServerGame
{
public:
    virtual ~ServerGame() = default;

    virtual bool OnInit() = 0;
    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnShutdown() = 0;
    
    // Optional override for custom network logic
    virtual NetworkControl* CreateNetworkControl() { return nullptr; }

    void SetEventService(EventService* es) { m_EventService = es; }
    void SetNetworkServices(NetworkService* ns, NetworkControl* nc) { m_NetService = ns; m_NetControl = nc; }

protected:
    SceneManager m_SceneManager;
    AssetManager m_AssetManager;
    EventService* m_EventService = nullptr;
    NetworkService* m_NetService = nullptr;
    NetworkControl* m_NetControl = nullptr;
};

#endif // SERVERGAME_H
