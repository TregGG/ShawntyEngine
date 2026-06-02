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

using json = nlohmann::json;

#define ENGINE_CLASS "TestPlayer"
#include "../core/enginedebug.h"

TestPlayer::TestPlayer(Scene* scene, const std::string& name, const SpriteSheetAsset* sheet, AssetManager* assets)
    : GameObject(scene, name, EntityCategory::Player)
{
    EntityID pID = m_ID;

    // Default player values
    glm::vec2 playerSize(1.0f, 1.0f);
    const SpriteSheetAsset* playerSheet = sheet;
    int playerFrameIndex = 0;
    float playerDrag = 2.0f;
    bool playerUseGravity = true;
    float playerGravityScale = 2.0f;

    // Default weapon values
    glm::vec2 weaponLocalPos(1.0f, -0.3f);
    glm::vec2 weaponSize(0.5f, 0.5f);
    const SpriteSheetAsset* weaponSheet = sheet;
    int weaponFrameIndex = 0;
    bool weaponIsTrigger = true;

    // Try loading player.prefab dynamically
    std::ifstream file("test_compiled/prefabs/player.prefab");
    if (file.is_open()) {
        try {
            json j = json::parse(file);
            if (j.contains("prefab")) {
                const auto& prefab = j["prefab"];
                if (prefab.contains("components")) {
                    const auto& comps = prefab["components"];
                    if (comps.contains("transform")) {
                        const auto& trans = comps["transform"];
                        if (trans.contains("size") && trans["size"].is_array() && trans["size"].size() == 2) {
                            playerSize.x = trans["size"][0].get<float>();
                            playerSize.y = trans["size"][1].get<float>();
                        }
                    }
                    if (comps.contains("sprite")) {
                        const auto& spr = comps["sprite"];
                        if (assets && spr.contains("objectId")) {
                            std::string objId = spr["objectId"].get<std::string>();
                            if (objId != "" && objId != "(none)") {
                                playerSheet = assets->GetSpriteSheet(objId);
                            }
                        }
                        playerFrameIndex = spr.value("frameIndex", playerFrameIndex);
                    }
                    if (comps.contains("rigidbody")) {
                        const auto& rb = comps["rigidbody"];
                        playerDrag = rb.value("drag", playerDrag);
                        playerUseGravity = rb.value("useGravity", playerUseGravity);
                        playerGravityScale = rb.value("gravityScale", playerGravityScale);
                    }
                }

                if (prefab.contains("children") && prefab["children"].is_array()) {
                    for (const auto& child : prefab["children"]) {
                        if (child.value("name", "") == "Weapon") {
                            if (child.contains("components")) {
                                const auto& comps = child["components"];
                                if (comps.contains("transform")) {
                                    const auto& trans = comps["transform"];
                                    if (trans.contains("localPosition") && trans["localPosition"].is_array() && trans["localPosition"].size() == 2) {
                                        weaponLocalPos.x = trans["localPosition"][0].get<float>();
                                        weaponLocalPos.y = trans["localPosition"][1].get<float>();
                                    }
                                    if (trans.contains("size") && trans["size"].is_array() && trans["size"].size() == 2) {
                                        weaponSize.x = trans["size"][0].get<float>();
                                        weaponSize.y = trans["size"][1].get<float>();
                                    }
                                }
                                if (comps.contains("sprite")) {
                                    const auto& spr = comps["sprite"];
                                    if (assets && spr.contains("objectId")) {
                                        std::string objId = spr["objectId"].get<std::string>();
                                        if (objId != "" && objId != "(none)") {
                                            weaponSheet = assets->GetSpriteSheet(objId);
                                        }
                                    }
                                    weaponFrameIndex = spr.value("frameIndex", weaponFrameIndex);
                                }
                                if (comps.contains("collider")) {
                                    const auto& col = comps["collider"];
                                    weaponIsTrigger = col.value("isTrigger", weaponIsTrigger);
                                }
                            }
                            break;
                        }
                    }
                }
            }
        } catch (...) {
            // Ignore parse errors, fallback to defaults
        }
    }

    // 1. Add Player Components
    TransformComponent pt;
    pt.position = glm::vec2(0.0f, 5.0f);
    pt.size = playerSize;
    scene->registry.AddComponent<TransformComponent>(pID, pt);

    SpriteComponent2D ps;
    ps.spriteSheet = playerSheet;
    ps.frameIndex = playerFrameIndex;
    ps.layer = Layer::Player;
    scene->registry.AddComponent<SpriteComponent2D>(pID, ps);

    ColliderComponent pc;
    pc.SetAutoBounds(true);
    scene->registry.AddComponent<ColliderComponent>(pID, pc);

    RigidBodyComponent prb;
    prb.SetType(BodyType::Dynamic);
    prb.SetDrag(playerDrag);
    prb.SetUseGravity(playerUseGravity);
    prb.SetGravityScale(playerGravityScale);
    scene->registry.AddComponent<RigidBodyComponent>(pID, prb);

    // 2. Create Child Weapon Entity
    m_WeaponID = scene->CreateEntity(EntityCategory::Environment, "Weapon");

    TransformComponent wt;
    wt.localPosition = weaponLocalPos;
    wt.position = pt.position + wt.localPosition;
    wt.size = weaponSize;
    scene->registry.AddComponent<TransformComponent>(m_WeaponID, wt);

    SpriteComponent2D ws;
    ws.spriteSheet = weaponSheet;
    ws.frameIndex = weaponFrameIndex;
    ws.layer = Layer::Player;
    scene->registry.AddComponent<SpriteComponent2D>(m_WeaponID, ws);

    ColliderComponent wc;
    wc.SetAutoBounds(true);
    wc.SetTrigger(weaponIsTrigger);
    
    // Set custom trigger callback to log details when hitting anything
    wc.SetOnTriggerEnter([scene](EntityID self, EntityID other) {
        // Disabled trigger logging to reduce spam
        (void)self; (void)other;
    });
    
    scene->registry.AddComponent<ColliderComponent>(m_WeaponID, wc);

    // 3. Parent-Child Relationship Components
    RelationshipComponent rel;
    rel.parent = pID;
    scene->registry.AddComponent<RelationshipComponent>(m_WeaponID, rel);
    
    scene->registry.GetComponent<TransformComponent>(m_WeaponID).parentTransform = 
        &scene->registry.GetComponent<TransformComponent>(pID);

    // Update Player relationship
    RelationshipComponent pRel;
    pRel.children.push_back(m_WeaponID);
    scene->registry.AddComponent<RelationshipComponent>(pID, pRel);
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
