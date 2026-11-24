#pragma once

#include "../core/servergame.h"
#include "../levels/scenemanager.h"
#include "../assets/assetmanager.h"
#include "../core/entityid.h"
#include "../services/contentsync.h"

class Scene;
class DataDrivenScene;

#include <unordered_map>
#include <string>

class ServerTestGame : public ServerGame
{
public:
    bool OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;

    NetworkControl* CreateNetworkControl() override;

    bool RunTransitionTest();

private:
    // Dynamic scene management
    std::unordered_map<std::string, DataDrivenScene*> m_Scenes;
    DataDrivenScene* GetOrCreateScene(const std::string& path);

    // Fixed timestep accumulator for networking ticks
    float m_NetworkTimeAccumulator = 0.0f;

    // Project content sent to clients
    ContentServer m_Content;
    std::string m_LoadedFingerprint;
    bool m_ReadyReported = false;
    std::string Fingerprint() const;
    void ReadServerSettings();
    bool PrepareReload(const std::string& scenePath);
};
