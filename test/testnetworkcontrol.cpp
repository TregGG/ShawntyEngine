#include "testnetworkcontrol.h"
#include "../assets/assetmanager.h"
#include "../core/input.h"
#include "../levels/scene.h"
#include <algorithm>
#include "testplayer.h"
#include <chrono>
#include "../objects/components/rigidbodycomponent.h"
#include "../objects/components/components.h"
#include <GLFW/glfw3.h>

TestNetworkControl::TestNetworkControl(AssetManager* assets) : m_Assets(assets) {}

TestNetworkControl::~TestNetworkControl() {}

EntityID TestNetworkControl::OnSpawnPlayer(ENetPeer* peer, bool isLocal) {
    auto playerObj = std::make_unique<TestPlayer>(m_Scene, "Player", m_Assets->GetSpriteSheet("player_box"), m_Assets);
    EntityID lID = playerObj->GetID();

    if (!isLocal && peer == nullptr) { // It's a remote proxy on the client
        if (m_Scene->registry.HasComponent<RigidBodyComponent>(lID)) {
            auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(lID);
            rb.SetType(BodyType::Kinematic);
            rb.SetUseGravity(false);
            rb.SetDrag(0.0f);
        }
    }
    
    m_ManagedObjects.push_back(std::move(playerObj));
    return lID;
}

void TestNetworkControl::OnDestroyPlayer(EntityID entity) {
    auto it = std::remove_if(m_ManagedObjects.begin(), m_ManagedObjects.end(), [entity](const std::unique_ptr<GameObject>& obj) {
        return obj->GetID() == entity;
    });
    if (it != m_ManagedObjects.end()) {
        m_ManagedObjects.erase(it, m_ManagedObjects.end());
    }
    
    if (m_Scene) {
        m_Scene->registry.DestroyRecursive(entity);
    }
}

uint16_t TestNetworkControl::OnGenerateInputMask(Input* input) {
    if (std::getenv("ENGINE_BOT")) {
        static auto startTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count() < 3) {
            return 0; // Wait for other bots to connect
        }

        if (m_Scene) {
            EntityID myID = GetLocalPlayerID();
            if (myID != 0 && m_Scene->registry.HasComponent<TransformComponent>(myID)) {
                auto const& trans = m_Scene->registry.GetComponent<TransformComponent>(myID);
                uint16_t mask = 0;
                if (trans.position.x < 5.0f) {
                    mask |= 8; // D (Move right)
                } else if (trans.position.x > 5.2f) {
                    mask |= 4; // A (Move left)
                }
                return mask;
            }
        }
        return 8; // Default move right
    }

    uint16_t mask = 0;
    if (input->IsKeyDown(GLFW_KEY_SPACE)) mask |= 1;
    if (input->IsKeyDown(GLFW_KEY_W))     mask |= 2;
    if (input->IsKeyDown(GLFW_KEY_A))     mask |= 4;
    if (input->IsKeyDown(GLFW_KEY_D))     mask |= 8;
    
    static bool fWasDown = false;
    bool fIsDown = input->IsKeyDown(GLFW_KEY_F);
    if (fIsDown && !fWasDown) {
        m_IsFrozen = !m_IsFrozen;
    }
    fWasDown = fIsDown;
    
    if (m_IsFrozen) {
        mask |= 16;
    }
    
    return mask;
}

#include "../levels/datadrivenscene.h"
#include "../services/base/raycast.h"

void TestNetworkControl::OnApplyInput(EntityID entity, uint16_t inputMask) {
    if (!m_Scene || !m_Scene->registry.HasComponent<RigidBodyComponent>(entity)) return;
    
    auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(entity);
    
    // Freeze Ability: turn into a static platform when F is held
    if (inputMask & 16) {
        rb.SetType(BodyType::Static);
        rb.SetVelocity(glm::vec2(0.0f));
        return;
    } else {
        // Only set to dynamic if we are on the server or if this is our local player
        if (entity == m_MyLocalPlayerID || m_MyLocalPlayerID == 0) { // m_MyLocalPlayerID == 0 means we are the server
            rb.SetType(BodyType::Dynamic);
        }
    }
    
    glm::vec2 vel = rb.GetVelocity();
    
    glm::vec2 moveVel(0.0f);
    if (inputMask & 4) moveVel.x -= 1.0f; // A
    if (inputMask & 8) moveVel.x += 1.0f; // D
    
    float moveSpeed = 5.0f;
    moveVel *= moveSpeed;
    vel.x = moveVel.x;
    
    bool isGrounded = false;
    bool ceilingHit = false;
    auto* ddScene = dynamic_cast<DataDrivenScene*>(m_Scene);
    if (ddScene && m_Scene->registry.HasComponent<TransformComponent>(entity)) {
        const auto& trans = m_Scene->registry.GetComponent<TransformComponent>(entity);
        const auto& physics = ddScene->GetPhysics();
        
        float width = trans.size.x;
        float height = trans.size.y;
        glm::vec2 bottomCenter = trans.position + glm::vec2(0.0f, -height * 0.5f);
        glm::vec2 topCenter = trans.position + glm::vec2(0.0f, height * 0.5f);
        
        RaycastHit hit;
        // Ground checks
        bool hitCenter = physics.Raycast(bottomCenter, glm::vec2(0.0f, -1.0f), 0.15f, hit, 0xFFFFFFFF, entity);
        bool hitLeft = physics.Raycast(bottomCenter + glm::vec2(-width * 0.5f + 0.05f, 0.0f), glm::normalize(glm::vec2(-0.2f, -1.0f)), 0.2f, hit, 0xFFFFFFFF, entity);
        bool hitRight = physics.Raycast(bottomCenter + glm::vec2(width * 0.5f - 0.05f, 0.0f), glm::normalize(glm::vec2(0.2f, -1.0f)), 0.2f, hit, 0xFFFFFFFF, entity);
        
        isGrounded = hitCenter || hitLeft || hitRight;
        
        // Ceiling check (don't jump if something is right on top of us)
        ceilingHit = physics.Raycast(topCenter, glm::vec2(0.0f, 1.0f), 0.15f, hit, 0xFFFFFFFF, entity);
    }
    
    // Constant raycast update for coyote time
    if (isGrounded) {
        m_CoyoteTicks[entity] = 4; // 4 ticks of coyote time
    } else {
        if (m_CoyoteTicks[entity] > 0) m_CoyoteTicks[entity]--;
    }
    
    if (inputMask & 1) { // Space
        if (!ceilingHit && m_CoyoteTicks[entity] > 0) {
            vel.y = 18.0f; // Jump
            m_CoyoteTicks[entity] = 0; // Consume the jump
        }
    }
    
    rb.SetVelocity(vel);
}

void TestNetworkControl::OnSceneChanged() {
    NetworkControl::OnSceneChanged();
    m_ManagedObjects.clear();
}
