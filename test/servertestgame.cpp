#include "servertestgame.h"

#include <cstring>
#include <algorithm>
#include "../core/logger.h"
#define ENGINE_CLASS "ServerTestGame"
#include "../core/enginedebug.h"
#include "../objects/components/rigidbodycomponent.h"
#include "../levels/datadrivenscene.h"
#include "../core/projectpaths.h"
#include "../services/networkservice.h"
#include "testnetworkcontrol.h"
#include "../core/devevents.h"
#include "../scripting/scriptengine.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <nlohmann/json.hpp>

// Forward declaration for asset compilation function if we want to share it
extern bool WriteTestCompiledAssets(const std::string& rootFolder);

NetworkControl* ServerTestGame::CreateNetworkControl() {
    return new TestNetworkControl(&m_AssetManager);
}

DataDrivenScene* ServerTestGame::GetOrCreateScene(const std::string& rawPath) {
    const std::string path = ProjectPaths::Resolve(rawPath);
    auto it = m_Scenes.find(path);
    if (it != m_Scenes.end()) {
        return it->second;
    }

    ENGINE_LOG("Loading dynamic scene on server: %s", path.c_str());
    DataDrivenScene* scene = new DataDrivenScene(&m_AssetManager, path, nullptr, m_EventService);
    scene->OnSceneReloadedCallback = [this, path]() {
        ENGINE_LOG("Server: Scene '%s' reloaded on disk! Resetting player network states...", path.c_str());
        if (m_NetControl) {
            m_NetControl->OnSceneChanged();
        }
        if (m_NetService) m_Content.Announce(*m_NetService);
    };
    scene->OnBeforeReload = [this, path]() { return PrepareReload(path); };

    m_Scenes[path] = scene;
    return scene;
}

// Written by the cloud backend. Never sent to clients.
void ServerTestGame::ReadServerSettings()
{
    std::ifstream file(ProjectPaths::Root() + "/server.json");
    if (!file) return;
    try {
        m_Content.SetJoinCode(nlohmann::json::parse(file).value("joinCode", ""));
    } catch (const std::exception& e) {
        ENGINE_ERROR("Invalid server.json: %s", e.what());
    }
}

// Changes whenever any project file does, including scripts
std::string ServerTestGame::Fingerprint() const
{
    namespace fs = std::filesystem;
    std::map<std::string, std::string> hashes;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(ProjectPaths::Root(), ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code entryError;
        if (!it->is_regular_file(entryError) || it->path().extension() == ".pyc" || it->path().extension() == ".tmp"
            || it->path().filename() == "server.json") continue;
        std::ifstream file(it->path(), std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        hashes[it->path().string()] = ToHex(Sha256(bytes.data(), bytes.size()));
    }
    std::string all;
    for (const auto& [path, hash] : hashes) all += path + '\0' + hash + '\n';
    return ToHex(Sha256(all.data(), all.size()));
}

// Checks a saved change before it replaces the running version.
bool ServerTestGame::PrepareReload(const std::string& scenePath)
{
    ReadServerSettings();
    const std::string fingerprint = Fingerprint();
    if (fingerprint == m_LoadedFingerprint) return false;

    std::string problems;
    try {
        std::ifstream file(scenePath);
        const auto scene = nlohmann::json::parse(file).at("scene");
        std::set<std::pair<std::string, std::string>> scripts;
        if (scene.contains("entities")) {
            for (const auto& entity : scene["entities"]) {
                if (!entity.contains("components") || !entity["components"].contains("script")) continue;
                const auto& script = entity["components"]["script"];
                const std::string path = script.value("path", ""), cls = script.value("class", "");
                if (!path.empty() && !cls.empty()) scripts.insert({path, cls});
            }
        }
        for (const auto& [path, cls] : scripts) {
            std::string error;
            if (!ScriptEngine::CheckScript(path, cls, error)) problems += "\n" + path + ": " + error;
        }
    } catch (const std::exception& e) {
        problems += "\n" + ProjectPaths::MakeRelative(scenePath) + ": " + e.what();
    }
    std::string contentError;
    if (problems.empty() && !m_Content.Load(ProjectPaths::Root(), contentError)) problems = "\n" + contentError;

    if (!problems.empty()) {
        ENGINE_ERROR("Change not applied:%s", problems.c_str());
        DevEvents::Emit("error", "Change not applied, the previous version keeps running:" + problems);
        return false;
    }

    m_LoadedFingerprint = fingerprint;
    DevEvents::ResetSeen();
    DevEvents::Emit("info", "Change applied");
    if (m_NetControl) m_NetControl->RememberPlayerPositions();
    return true;
}

bool ServerTestGame::OnInit()
{
    const std::string compiledRoot = ProjectPaths::Root();

    if (!m_AssetManager.Initialize(compiledRoot, true))
    {
        ENGINE_ERROR("AssetManager failed to initialize");
        return false;
    }

    // Register global change scene callback for Python scripts
    g_ChangeSceneCallback = [this](const std::string& scenePath) {
        ENGINE_LOG("g_ChangeSceneCallback on server: %s", scenePath.c_str());
        DataDrivenScene* targetScene = GetOrCreateScene(scenePath);
        if (targetScene) {
            if (m_NetControl) {
                m_NetControl->ForgetPlayerPositions();
                m_NetControl->OnSceneChanged();
            }
            m_SceneManager.SetActiveScene(targetScene);
            if (m_NetControl) {
                m_NetControl->BindScene(targetScene, nullptr);
            }
        }
    };

    ReadServerSettings();
    m_LoadedFingerprint = Fingerprint();
    std::string contentError;
    if (!m_Content.Load(ProjectPaths::Root(), contentError)) {
        ENGINE_ERROR("Cannot share project content: %s", contentError.c_str());
        DevEvents::Emit("error", "Players cannot download this project: " + contentError);
    }
    if (m_NetControl) m_NetControl->SetContentServer(&m_Content);

    // ---- Create Initial Scene from JSON ----
    DataDrivenScene* initialScene = GetOrCreateScene(ProjectPaths::IsDefault()
        ? "test_compiled/scenes/testscene1.scene" : "scenes/main.scene");
    m_SceneManager.SetInitialScene(initialScene);

    // ---- Wire networking ----
    if (m_NetControl) {
        m_NetControl->BindScene(initialScene, nullptr);
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
            // Server always enables live sync UDP listener on the active scene
            activeDD->SetUdpListenerEnabled(true);
            if (!m_ReadyReported) {
                m_ReadyReported = true;
                DevEvents::Emit("info", "Game server ready");
            }
        }
    }
}

void ServerTestGame::OnShutdown()
{
    m_SceneManager.Shutdown();

    for (auto& [path, scene] : m_Scenes) {
        delete scene;
    }
    m_Scenes.clear();

    m_AssetManager.Shutdown();
    ENGINE_LOG("ServerTestGame Shutdown");
}

bool ServerTestGame::RunTransitionTest() {
    ENGINE_LOG("=== Running Transition Test ===");

    DataDrivenScene* activeDD = dynamic_cast<DataDrivenScene*>(m_SceneManager.GetActiveScene());
    if (!activeDD) {
        ENGINE_ERROR("No active data driven scene!");
        return false;
    }

    // 1. Locate the trigger entity
    EntityID triggerId = activeDD->GetEntityByEditorId("scene_trigger");
    if (triggerId == 0) {
        ENGINE_ERROR("Could not find trigger entity!");
        return false;
    }

    // 2. Spawn a mock player directly in the registry
    EntityID playerId = activeDD->registry.Create(EntityCategory::Player, "MockPlayer", "Scene");
    
    // 3. Register the mock player in the network control
    TestNetworkControl* netControl = dynamic_cast<TestNetworkControl*>(m_NetControl);
    if (!netControl) {
        ENGINE_ERROR("NetControl is not TestNetworkControl!");
        return false;
    }
    
    // Use an arbitrary mock pointer for the peer
    ENetPeer* mockPeer = reinterpret_cast<ENetPeer*>(0x55555555);
    netControl->AddMockPlayer(mockPeer, playerId);

    // Verify active players returns our mock player
    std::vector<EntityID> players = netControl->GetActivePlayerEntities();
    if (players.size() != 1 || players[0] != playerId) {
        ENGINE_ERROR("Mock player was not registered correctly in NetControl!");
        return false;
    }

    // 4. Trigger overlap enter callback manually
    ENGINE_LOG("Simulating OnTriggerEnter for mock player on trigger");
    activeDD->GetScriptEngine().CallOnTriggerEnter(triggerId, playerId);
    
    // 5. Verify the scene transition occurred!
    DataDrivenScene* newActiveScene = dynamic_cast<DataDrivenScene*>(m_SceneManager.GetActiveScene());
    if (!newActiveScene) {
        ENGINE_ERROR("Active scene is null after transition!");
        return false;
    }

    std::string scenePath = newActiveScene->GetSceneFilePath();
    ENGINE_LOG("After transition, active scene path: %s", scenePath.c_str());

    if (scenePath != "test_compiled/scenes/testscene2.scene") {
        ENGINE_ERROR("Transition failed! Active scene is not testscene2.scene, got: %s", scenePath.c_str());
        return false;
    }

    ENGINE_LOG("=== Transition Test Passed! ===");
    return true;
}

