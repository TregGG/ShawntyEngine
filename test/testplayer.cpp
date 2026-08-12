#include "testplayer.h"
#include "../assets/assetmanager.h"
#include "../levels/scene.h"
#include "../objects/components/spriterenderer2d.h"
#include "../objects/components/collidercomponent.h"
#include "../objects/components/rigidbodycomponent.h"
#include "../core/input.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../serialization/sceneserializer.h"

using json = nlohmann::json;

#define ENGINE_CLASS "TestPlayer"
#include "../core/enginedebug.h"

TestPlayer::TestPlayer(Scene* scene, const std::string& name, const SpriteSheetAsset* sheet, AssetManager* assets)
    : GameObject(scene, name, EntityCategory::Player)
{
    // Determine which prefab to use via cascading fallback:
    //   1. Per-scene override (from scene JSON settings.playerPrefab)
    //   2. Project default  (from test_compiled/project.json -> defaultPlayerPrefab)
    //   3. Hardcoded fallback ("player")
    std::string prefabName;

    // 1. Check scene-level override
    if (!scene->GetPlayerPrefab().empty()) {
        prefabName = scene->GetPlayerPrefab();
        ENGINE_LOG("Using scene-level player prefab: %s", prefabName.c_str());
    } else {
        // 2. Check project.json
        std::ifstream projFile("test_compiled/project.json");
        if (projFile.is_open()) {
            try {
                json projSettings = json::parse(projFile);
                if (projSettings.contains("defaultPlayerPrefab") &&
                    projSettings["defaultPlayerPrefab"].is_string() &&
                    !projSettings["defaultPlayerPrefab"].get<std::string>().empty()) {
                    prefabName = projSettings["defaultPlayerPrefab"].get<std::string>();
                    ENGINE_LOG("Using project default player prefab: %s", prefabName.c_str());
                }
            } catch (const json::parse_error& e) {
                ENGINE_WARN("Failed to parse project.json: %s", e.what());
            }
        }

        // 3. Fallback
        if (prefabName.empty()) {
            prefabName = "player";
            ENGINE_LOG("Using fallback player prefab: %s", prefabName.c_str());
        }
    }

    std::string prefabPath = "test_compiled/prefabs/" + prefabName + ".prefab";

    // Try loading the prefab dynamically using the SceneSerializer onto our existing Player category entity
    EntityID newID = SceneSerializer::InstantiatePrefab(prefabPath, scene, assets, {0.0f, 0.0f}, nullptr, m_ID);
    
    if (newID != 0) {
        m_ID = newID;
    } else {
        // Fallback if prefab failed to load
        ENGINE_ERROR("Failed to load player prefab, falling back to creating default Player entity");
        m_ID = m_Scene->CreateEntity(EntityCategory::Player, name);
        
        TransformComponent pt;
        pt.position = glm::vec2(0.0f, 5.0f);
        pt.size = glm::vec2(1.0f, 1.0f);
        scene->registry.AddComponent<TransformComponent>(m_ID, pt);

        SpriteComponent2D ps;
        ps.spriteSheet = sheet;
        ps.frameIndex = 0;
        ps.layer = Layer::Player;
        scene->registry.AddComponent<SpriteComponent2D>(m_ID, ps);

        ColliderComponent pc;
        pc.SetAutoBounds(true);
        scene->registry.AddComponent<ColliderComponent>(m_ID, pc);

        RigidBodyComponent prb;
        prb.SetType(BodyType::Dynamic);
        prb.SetDrag(2.0f);
        prb.SetUseGravity(true);
        prb.SetGravityScale(2.0f);
        scene->registry.AddComponent<RigidBodyComponent>(m_ID, prb);
    }
}

void TestPlayer::PassInput(const Input* input)
{
    m_Input = input;
}

void TestPlayer::Update(float deltaTime)
{
    (void)deltaTime; // Unused for input but matches update signature
    glm::vec2 pushForce(0.0f);

    if (m_Input)
    {
        // A/D for lateral movement
        if (m_Input->IsKeyDown(GLFW_KEY_A)) { pushForce.x -= 1.0f; }
        if (m_Input->IsKeyDown(GLFW_KEY_D)) { pushForce.x += 1.0f; }
        
        static bool spacePressed = false;
        if (m_Input->IsKeyDown(GLFW_KEY_SPACE)) {
            if (!spacePressed) {
                // Jump!
                if (m_Scene->registry.HasComponent<RigidBodyComponent>(m_ID)) {
                    auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(m_ID);
                    rb.SetVelocity(glm::vec2(rb.GetVelocity().x, 18.0f));
                }
                spacePressed = true;
            }
        } else {
            spacePressed = false;
        }

        // Log input values occasionally for diagnostics
        // static int frameCounter = 0;
        // if (++frameCounter % 60 == 0) {
        //     ENGINE_LOG("Diag Player - m_Input: %p | A: %d | D: %d | Space: %d | pushForce.x: %.2f", 
        //         m_Input,
        //         m_Input->IsKeyDown(GLFW_KEY_A),
        //         m_Input->IsKeyDown(GLFW_KEY_D),
        //         m_Input->IsKeyDown(GLFW_KEY_SPACE),
        //         pushForce.x);
        // }
    }

    // Apply input-driven movement to Player
    if (m_Input && m_Scene->registry.HasComponent<RigidBodyComponent>(m_ID))
    {
        auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(m_ID);
        glm::vec2 currentVel = rb.GetVelocity();
        // Match server-side direct velocity setting (m_MoveSpeed is 5.0f)
        rb.SetVelocity(glm::vec2(pushForce.x * 5.0f, currentVel.y));
    }
}
