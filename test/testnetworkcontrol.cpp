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
    auto playerObj = std::make_unique<TestPlayer>(m_Scene, "Player", m_Assets->GetSpriteSheet("testobj"));
    EntityID lID = playerObj->GetID();
    
    // Set spawn position
    if (m_Scene->registry.HasComponent<TransformComponent>(lID)) {
        auto& trans = m_Scene->registry.GetComponent<TransformComponent>(lID);
        trans.position = glm::vec2(0.0f, 2.0f);
    }

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
    EntityID weaponID = 0;
    
    for (auto const& obj : m_ManagedObjects) {
        if (obj->GetID() == entity) {
            if (auto* player = dynamic_cast<TestPlayer*>(obj.get())) {
                weaponID = player->GetWeaponID();
            }
            break;
        }
    }
    
    auto it = std::remove_if(m_ManagedObjects.begin(), m_ManagedObjects.end(), [entity](const std::unique_ptr<GameObject>& obj) {
        return obj->GetID() == entity;
    });
    if (it != m_ManagedObjects.end()) {
        m_ManagedObjects.erase(it, m_ManagedObjects.end());
    }
    
    if (m_Scene) {
        m_Scene->registry.Destroy(entity);
        if (weaponID != 0) {
            m_Scene->registry.Destroy(weaponID);
        }
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
    return mask;
}

void TestNetworkControl::OnApplyInput(EntityID entity, uint16_t inputMask) {
    if (!m_Scene || !m_Scene->registry.HasComponent<RigidBodyComponent>(entity)) return;
    
    auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(entity);
    glm::vec2 vel = rb.GetVelocity();
    
    glm::vec2 moveVel(0.0f);
    if (inputMask & 4) moveVel.x -= 1.0f; // A
    if (inputMask & 8) moveVel.x += 1.0f; // D
    
    float moveSpeed = 5.0f;
    moveVel *= moveSpeed;
    vel.x = moveVel.x;
    
    if (inputMask & 1) { // Space
        vel.y = 18.0f; // Jump
    }
    
    rb.SetVelocity(vel);
}

void TestNetworkControl::OnSceneChanged() {
    NetworkControl::OnSceneChanged();
    m_ManagedObjects.clear();
}
