#pragma once

#include "../core/servergame.h"
#include "../levels/scenemanager.h"
#include "../assets/assetmanager.h"
#include "../core/entityid.h"

class Scene;
class DataDrivenScene;

class ServerTestGame : public ServerGame
{
public:
    bool OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;

    NetworkControl* CreateNetworkControl() override;

private:
    // Data-driven scenes
    DataDrivenScene* m_DDScene1 = nullptr;
    DataDrivenScene* m_DDScene2 = nullptr;

    // Track the scene trigger entity for scene transition
    EntityID m_TriggerID = 0;

    // Fixed timestep accumulator for networking ticks
    float m_NetworkTimeAccumulator = 0.0f;
};
