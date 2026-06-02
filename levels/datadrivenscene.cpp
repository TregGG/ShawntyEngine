#include "datadrivenscene.h"
#include "../serialization/sceneserializer.h"
#include "../assets/assetmanager.h"
#include "../core/input.h"
#include "../objects/components/collidercomponent.h"
#include "../objects/components/scriptcomponent.h"
#include <algorithm>
#include <GLFW/glfw3.h>

#define ENGINE_CLASS "DataDrivenScene"
#include "../core/enginedebug.h"

DataDrivenScene::DataDrivenScene(AssetManager* assets, const std::string& sceneFilePath, FontEngine* fontEngine, EventService* eventService)
    : Scene(assets), m_SceneFilePath(sceneFilePath), m_FontEngine(fontEngine), m_EventService(eventService)
{
}

void DataDrivenScene::OnEnter()
{
    registry.Init();
    m_Physics.Init();
    m_Physics.BindRegistry(&registry);

    if (!m_Assets) {
        ENGINE_ERROR("OnEnter failed: no AssetManager provided");
        return;
    }

    auto result = SceneSerializer::LoadScene(m_SceneFilePath, this, m_Assets, m_FontEngine, m_EventService);
    if (result.success) {
        m_EditorIdMap = std::move(result.editorIdMap);
        ENGINE_LOG("DataDrivenScene entered: %s (%zu entities loaded)",
                   m_SceneFilePath.c_str(), m_EditorIdMap.size());
    } else {
        ENGINE_ERROR("Failed to load scene: %s", result.errorMessage.c_str());
        return;
    }

    // --- Initialize ScriptEngine and attach scripts ---
    m_ScriptEngine.Init();
    m_ScriptEngine.BindRegistry(&registry);
    m_ScriptEngine.SetInput(m_Input);

    // Find all entities with ScriptComponent and attach their scripts
    for (int c = 0; c < static_cast<int>(EntityCategory::Count); ++c) {
        for (uint32_t idx : registry.GetEntities(static_cast<EntityCategory>(c))) {
            EntityID eid = MakeEntityID(idx, 0);
            if (registry.HasComponent<ScriptComponent>(eid)) {
                auto& sc = registry.GetComponent<ScriptComponent>(eid);
                m_ScriptEngine.AttachScript(eid, sc);
            }
        }
    }

    // --- Wire trigger callbacks to forward to ScriptEngine ---
    for (EntityID e : registry.ViewPhysicsObjects()) {
        if (registry.HasComponent<ColliderComponent>(e)) {
            auto& col = registry.GetComponent<ColliderComponent>(e);
            if (col.IsTrigger()) {
                col.SetOnTriggerEnter([this](EntityID self, EntityID other) {
                    // Notify both entities' scripts about the trigger event
                    m_ScriptEngine.CallOnTriggerEnter(self, other);
                    m_ScriptEngine.CallOnTriggerEnter(other, self);
                });
                col.SetOnTriggerExit([this](EntityID self, EntityID other) {
                    m_ScriptEngine.CallOnTriggerExit(self, other);
                    m_ScriptEngine.CallOnTriggerExit(other, self);
                });
            }
        }
    }

    // Call OnStart on all attached scripts
    m_ScriptEngine.StartPendingScripts();
}

void DataDrivenScene::OnExit()
{
    m_ScriptEngine.Shutdown();
    m_EditorIdMap.clear();
    m_GameObjects.clear();
    m_Physics.Shutdown();
    registry.Shutdown();
}

void DataDrivenScene::Update(float deltatime)
{
    deltatime = std::min(deltatime, 0.1f);

    // Update script engine input pointer (may change between frames)
    m_ScriptEngine.SetInput(m_Input);

    // Camera movement with arrow keys
    glm::vec2 cameraDir(0.0f);
    if (m_Input) {
        if (m_Input->IsKeyDown(GLFW_KEY_UP))    cameraDir.y += 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_DOWN))  cameraDir.y -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_LEFT))  cameraDir.x -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_RIGHT)) cameraDir.x += 1.0f;
    }

    // Fixed 60Hz physics tick
    float tickInterval = 1.0f / 60.0f;
    m_TimeAccumulator += deltatime;
    while (m_TimeAccumulator >= tickInterval) {
        m_Physics.Update(tickInterval);
        m_TimeAccumulator -= tickInterval;
    }

    // --- Run Python scripts after physics ---
    m_ScriptEngine.UpdateAll(deltatime);

    // Camera movement
    float moveSpeed = 5.0f;
    glm::vec2 newPos = m_Camera.GetCameraPosition() + cameraDir * moveSpeed * deltatime;
    m_Camera.SetCameraPosition(newPos.x, newPos.y);

    // Update animators
    for (EntityID e : registry.ViewAnimators()) {
        auto& animator = registry.GetComponent<AnimatorComponent>(e);
        if (animator.IsActive()) {
            animator.Update(deltatime);
        }
    }

    registry.Update(deltatime);
}

void DataDrivenScene::Reload()
{
    ENGINE_LOG("Reloading scene from: %s", m_SceneFilePath.c_str());
    OnExit();
    OnEnter();
    ENGINE_LOG("Scene reloaded successfully");
}

EntityID DataDrivenScene::GetEntityByEditorId(const std::string& editorId) const
{
    auto it = m_EditorIdMap.find(editorId);
    if (it != m_EditorIdMap.end()) return it->second;
    return 0;
}

void DataDrivenScene::BuildDebugRenderables(std::vector<DebugRect>& outDebugRects) const
{
#ifdef ENGINE_DEBUG
    outDebugRects.clear();
    for (EntityID e : registry.ViewPhysicsObjects()) {
        const auto& col = registry.GetComponent<ColliderComponent>(e);
        const auto& trans = registry.GetComponent<TransformComponent>(e);

        auto b = col.GetBounds(trans);
        glm::vec2 size(b.maxX - b.minX, b.maxY - b.minY);
        glm::vec3 pos(b.minX + size.x * 0.5f, b.minY + size.y * 0.5f, 0.0f);

        glm::vec3 cColor = col.IsTrigger() ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        outDebugRects.push_back({pos, size, cColor});
    }
#else
    (void)outDebugRects;
#endif
}

void DataDrivenScene::BuildDebugLines(std::vector<DebugLine>& outDebugLines) const
{
    // Draw crosshair at origin
    outDebugLines.push_back({glm::vec2(-0.5f, 0.0f), glm::vec2(0.5f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)});
    outDebugLines.push_back({glm::vec2(0.0f, -0.5f), glm::vec2(0.0f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f)});
}
