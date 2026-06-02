#include "testplayer.h"
#include "../levels/scene.h"
#include "../objects/components/spriterenderer2d.h"
#include "../objects/components/collidercomponent.h"
#include "../objects/components/rigidbodycomponent.h"
#include "../core/input.h"
#include <GLFW/glfw3.h>

#define ENGINE_CLASS "TestPlayer"
#include "../core/enginedebug.h"

TestPlayer::TestPlayer(Scene* scene, const std::string& name, const SpriteSheetAsset* sheet)
    : GameObject(scene, name, EntityCategory::Player)
{
    EntityID pID = m_ID;

    // 1. Add Player Components
    TransformComponent pt;
    pt.position = glm::vec2(0.0f, 5.0f);
    pt.size = glm::vec2(1.0f, 1.0f);
    scene->registry.AddComponent<TransformComponent>(pID, pt);

    SpriteComponent2D ps;
    ps.spriteSheet = sheet;
    ps.frameIndex = 0;
    ps.layer = Layer::Player;
    scene->registry.AddComponent<SpriteComponent2D>(pID, ps);

    ColliderComponent pc;
    pc.SetAutoBounds(true);
    scene->registry.AddComponent<ColliderComponent>(pID, pc);

    RigidBodyComponent prb;
    prb.SetType(BodyType::Dynamic);
    prb.SetDrag(2.0f);
    prb.SetUseGravity(true);
    prb.SetGravityScale(2.0f);
    scene->registry.AddComponent<RigidBodyComponent>(pID, prb);

    // 2. Create Child Weapon Entity
    m_WeaponID = scene->CreateEntity(EntityCategory::Environment, "Weapon");

    TransformComponent wt;
    wt.localPosition = glm::vec2(1.0f, -0.3f); // Offset to the right and slightly down
    wt.position = pt.position + wt.localPosition; // Initial world pos
    wt.size = glm::vec2(0.5f, 0.5f);
    scene->registry.AddComponent<TransformComponent>(m_WeaponID, wt);

    SpriteComponent2D ws;
    ws.spriteSheet = sheet;
    ws.frameIndex = 0;
    ws.layer = Layer::Player;
    scene->registry.AddComponent<SpriteComponent2D>(m_WeaponID, ws);

    ColliderComponent wc;
    wc.SetAutoBounds(true);
    wc.SetTrigger(true); // Weapon acts as a trigger/hitbox
    
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
