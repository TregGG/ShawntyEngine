#pragma once

#include "../core/game.h"
#include "../levels/scenemanager.h"
#include "../render/rendermanager.h"
#include "../assets/assetmanager.h"

class Scene;
class DataDrivenScene;

class TestGame : public Game
{
public:
    bool OnInit() override;
    void OnInput(const Input& input) override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnShutdown() override;

    NetworkControl* CreateNetworkControl() override;

private:
    // Helper: Create UI for host/join (called after scene enters)
    void CreateNetworkUI(Scene* scene);

    DataDrivenScene* m_DDScene1 = nullptr;
    DataDrivenScene* m_DDScene2 = nullptr;

    // Networking UI state
    class UIText* m_StatusText = nullptr;
    bool m_UIHidden = false;

    // Track the scene trigger entity for scene transition
    EntityID m_TriggerID = 0;

    // Fixed timestep accumulator for networking ticks
    float m_NetworkTimeAccumulator = 0.0f;
};