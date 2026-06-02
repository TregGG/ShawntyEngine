#pragma once

#include "../core/game.h"
#include "../levels/scenemanager.h"
#include "../render/rendermanager.h"
#include "../assets/assetmanager.h"

#include <unordered_map>
#include <string>

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

    // Dynamic scene management
    std::unordered_map<std::string, DataDrivenScene*> m_Scenes;
    DataDrivenScene* GetOrCreateScene(const std::string& path);
    void HandleSceneReloaded(const std::string& path, DataDrivenScene* scene);

    // Networking UI state
    class UIText* m_StatusText = nullptr;
    bool m_UIHidden = false;
    
    // Fixed timestep accumulator for networking ticks
    float m_NetworkTimeAccumulator = 0.0f;
};