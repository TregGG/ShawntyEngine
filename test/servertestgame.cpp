#include "servertestgame.h"

#include <cstring>
#include <algorithm>
#include "../core/logger.h"
#define ENGINE_CLASS "ServerTestGame"
#include "../core/enginedebug.h"
#include "../objects/components/rigidbodycomponent.h"
#include "../levels/datadrivenscene.h"
#include "../services/networkservice.h"
#include "testnetworkcontrol.h"

// Forward declaration for asset compilation function if we want to share it
extern bool WriteTestCompiledAssets(const std::string& rootFolder);

NetworkControl* ServerTestGame::CreateNetworkControl() {
    return new TestNetworkControl(&m_AssetManager);
}

bool ServerTestGame::OnInit()
{
    ENGINE_LOG("ServerTestGame::OnInit");

    const std::string compiledRoot = "test_compiled";
    if (!WriteTestCompiledAssets(compiledRoot))
    {
        ENGINE_ERROR("Failed to write compiled assets");
        return false;
    }

    if (!m_AssetManager.Initialize(compiledRoot, true))
    {
        ENGINE_ERROR("AssetManager failed to initialize");
        return false;
    }

    // ---- Create DataDrivenScenes from JSON ----
    ENGINE_LOG("Creating DataDrivenScenes from JSON files (Server Mode)");
    
    // Note: passing nullptr for font engine on server
    m_DDScene1 = new DataDrivenScene(&m_AssetManager, "test_compiled/scenes/testscene1.scene", nullptr, m_EventService);
    m_DDScene2 = new DataDrivenScene(&m_AssetManager, "test_compiled/scenes/testscene2.scene", nullptr, m_EventService);

    m_SceneManager.SetInitialScene(m_DDScene1);

    // ---- Wire networking ----
    if (m_NetControl) {
        m_NetControl->BindScene(m_DDScene1, nullptr);
    }

    // Look up the trigger entity from the loaded scene
    m_TriggerID = m_DDScene1->GetEntityByEditorId("scene_trigger");
    if (m_TriggerID == 0) {
        ENGINE_WARN("Could not find 'scene_trigger' entity in scene");
    }

    if (m_NetControl && m_TriggerID != 0) {
        ENGINE_LOG("Server mode: trigger-based scene transition enabled");
    }

    return true;
}

void ServerTestGame::OnUpdate(float deltaTime)
{
    if (m_NetControl) {
        float netTickRate = 1.0f / 60.0f; // 60 ticks per second for lower input latency
        m_NetworkTimeAccumulator += deltaTime;
        while (m_NetworkTimeAccumulator >= netTickRate) {
            m_NetControl->Tick(netTickRate);
            m_NetworkTimeAccumulator -= netTickRate;
        }
    }

    // Scene update (includes Physics ticking inside DataDrivenScene)
    m_SceneManager.Update(deltaTime);
    
    Scene* currentScene = m_SceneManager.GetActiveScene();
    if (currentScene) {

        // Server physics config update from network cvars
        if (auto* activeDD = dynamic_cast<DataDrivenScene*>(currentScene)) {
            // ---- Server: scene transition when all players in trigger ----
            if (m_NetControl && m_TriggerID != 0) {
                std::vector<EntityID> players = m_NetControl->GetActivePlayerEntities();
                if (!players.empty()) {
                    bool allInTrigger = true;
                    for (EntityID pID : players) {
                        if (!activeDD->GetPhysics().IsColliding(pID, m_TriggerID)) {
                            allInTrigger = false;
                            break;
                        }
                    }
                    if (allInTrigger) {
                        ENGINE_LOG("Server: All players are in the trigger zone! Transitioning...");
                        m_NetControl->OnSceneChanged();
                        
                        m_SceneManager.SetActiveScene(m_DDScene2);
                        m_NetControl->BindScene(m_DDScene2, nullptr);
                        m_TriggerID = 0; // Disable trigger after transition
                    }
                }
            }
        }
    }
}

void ServerTestGame::OnShutdown()
{
    m_SceneManager.Shutdown();

    if (m_DDScene1)
    {
        delete m_DDScene1;
        m_DDScene1 = nullptr;
    }
    if (m_DDScene2)
    {
        delete m_DDScene2;
        m_DDScene2 = nullptr;
    }

    m_AssetManager.Shutdown();
    ENGINE_LOG("ServerTestGame Shutdown");
}
