#pragma once
#include "scene.h"
#include <string>
#include <unordered_map>
#include "../services/base/physics/physicssystem.h"
#include "../scripting/scriptengine.h"

class AssetManager;

class FontEngine;
class EventService;

class DataDrivenScene : public Scene
{
public:
    DataDrivenScene(AssetManager* assets, const std::string& sceneFilePath, FontEngine* fontEngine = nullptr, EventService* eventService = nullptr);
    ~DataDrivenScene() override = default;

    void OnEnter() override;
    void OnExit() override;
    void Update(float deltatime) override;

    void BuildDebugRenderables(std::vector<DebugRect>& outDebugRects) const override;
    void BuildDebugLines(std::vector<DebugLine>& outDebugLines) const override;

    // Reload the scene from disk (for hot reload later)
    void Reload();

    // Editor ID → Runtime EntityID lookup
    EntityID GetEntityByEditorId(const std::string& editorId) const;

    // Access physics system for external configuration
    PhysicsSystem& GetPhysics() { return m_Physics; }
    const PhysicsSystem& GetPhysics() const { return m_Physics; }

    // Access scripting system
    ScriptEngine& GetScriptEngine() { return m_ScriptEngine; }
    const ScriptEngine& GetScriptEngine() const { return m_ScriptEngine; }

    // Get the scene file path
    const std::string& GetSceneFilePath() const { return m_SceneFilePath; }

    // UDP Listener controls
    void SetUdpListenerEnabled(bool enabled);
    std::function<void()> OnSceneReloadedCallback;

private:
    void InitUdpListener();
    void ShutdownUdpListener();
    void PollUdpReload();

    std::string m_SceneFilePath;
    PhysicsSystem m_Physics;
    ScriptEngine m_ScriptEngine;
    float m_TimeAccumulator = 0.0f;

    std::unordered_map<std::string, EntityID> m_EditorIdMap;

    FontEngine* m_FontEngine = nullptr;
    EventService* m_EventService = nullptr;

    // UDP Live Sync Socket
    int m_UdpSocket = -1;
    bool m_UdpListenerEnabled = false;
};
