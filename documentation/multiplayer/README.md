# Developer Guide: Building Multiplayer Games

Welcome to the ShawntyEngine multiplayer documentation. By leveraging ENet, server-authoritative physics, and client-side prediction, the engine allows you to build responsive real-time multiplayer games.

This guide provides a step-by-step tutorial on how to implement custom multiplayer gameplay.

---

## 1. Directory Structure

All multiplayer documentation files are structured as follows:
* [networking_and_packets.md](file:///home/tregg/Public/Projects/engine/framework/documentation/multiplayer/networking_and_packets.md): ENet protocols, packet mappings, handshake flow, and disconnect lifecycle.
* [architecture_physics_scenes.md](file:///home/tregg/Public/Projects/engine/framework/documentation/multiplayer/architecture_physics_scenes.md): ECS registry state keeping, client input prediction, server physics, desync snaps, and scene transitions.

---

## 2. Step-by-Step Tutorial: Creating a Custom Multiplayer Game

To build a multiplayer game using this engine, you must implement three primary elements:
1. A custom **Player GameObject** that holds its components.
2. A custom **NetworkControl Service** that governs input generation and physics simulation.
3. Custom **Scenes** to load assets, draw maps, and trigger transitions.

### Step A: Implement a Custom Player
Define a `Player` class inheriting `GameObject`. In the constructor, assign components like `TransformComponent`, `SpriteComponent2D`, `ColliderComponent`, and `RigidBodyComponent` to the ECS registry.

```cpp
// player.h
#pragma once
#include "objects/gameobject.h"

class Player : public GameObject {
public:
    Player(Scene* scene, const std::string& name) : GameObject(scene, name) {
        EntityID id = GetID();
        
        // 1. Transform component
        TransformComponent t;
        t.position = glm::vec2(0.0f, 0.0f);
        t.size = glm::vec2(1.0f, 1.0f);
        scene->registry.AddComponent<TransformComponent>(id, t);
        
        // 2. Physics Collider
        ColliderComponent c;
        c.SetAutoBounds(true);
        scene->registry.AddComponent<ColliderComponent>(id, c);
        
        // 3. Authoritative Rigid Body
        RigidBodyComponent rb;
        rb.SetType(BodyType::Dynamic);
        rb.SetMass(1.0f);
        scene->registry.AddComponent<RigidBodyComponent>(id, rb);
    }
};
```

### Step B: Inherit `NetworkControl`
Override the virtual callbacks in `NetworkControl` to manage how players spawn, how client keys are captured, and how input masks translate into velocity changes:

```cpp
// customnetworkcontrol.h
#pragma once
#include "services/networkcontrol.h"

class CustomNetworkControl : public NetworkControl {
protected:
    // 1. Spawning Logic
    EntityID OnSpawnPlayer(ENetPeer* peer, bool isLocal) override {
        // Instantiate our custom player in the current active scene
        auto player = std::make_unique<Player>(m_Scene, "CustomPlayer");
        EntityID id = player->GetID();
        
        // If it's a remote client proxy on a client, disable gravity/simulation
        if (!isLocal && peer == nullptr) {
            auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(id);
            rb.SetType(BodyType::Kinematic);
            rb.SetUseGravity(false);
        }
        
        m_ManagedObjects.push_back(std::move(player));
        return id;
    }
    
    // 2. Destroying Logic
    void OnDestroyPlayer(EntityID entity) override {
        // Clear references
        m_ManagedObjects.erase(std::remove_if(m_ManagedObjects.begin(), m_ManagedObjects.end(), 
            [entity](const std::unique_ptr<GameObject>& obj) {
                return obj->GetID() == entity;
            }), m_ManagedObjects.end());
            
        if (m_Scene) m_Scene->registry.Destroy(entity);
    }
    
    // 3. Capturing Client Keypresses
    uint16_t OnGenerateInputMask(Input* input) override {
        uint16_t mask = 0;
        if (input->IsKeyDown(GLFW_KEY_W))     mask |= 1; // Up
        if (input->IsKeyDown(GLFW_KEY_S))     mask |= 2; // Down
        if (input->IsKeyDown(GLFW_KEY_A))     mask |= 4; // Left
        if (input->IsKeyDown(GLFW_KEY_D))     mask |= 8; // Right
        return mask;
    }
    
    // 4. Authoritatively Applying Inputs (executed on both Client and Server)
    void OnApplyInput(EntityID entity, uint16_t inputMask) override {
        if (!m_Scene || !m_Scene->registry.HasComponent<RigidBodyComponent>(entity)) return;
        auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(entity);
        
        glm::vec2 vel(0.0f);
        if (inputMask & 4) vel.x -= 5.0f; // Left
        if (inputMask & 8) vel.x += 5.0f; // Right
        if (inputMask & 1) vel.y += 5.0f; // Up
        if (inputMask & 2) vel.y -= 5.0f; // Down
        
        rb.SetVelocity(vel);
    }
};
```

### Step C: Create custom levels and transitions
To change levels, design scenes with triggers and bind them to transition callbacks:

```cpp
// level1.h
#pragma once
#include "levels/scene.h"

class Level1 : public Scene {
public:
    EntityID m_TriggerID;
    std::function<void()> OnTransitionTriggered;

    void OnEnter() override {
        registry.Init();
        m_Physics.Init();
        m_Physics.BindRegistry(&registry);
        m_NetControl->BindScene(this, m_Input);

        // Spawn a trigger zone at x = 10.0f
        m_TriggerID = CreateEntity(EntityCategory::Environment, "TriggerZone");
        
        TransformComponent t;
        t.position = glm::vec2(10.0f, 0.0f);
        t.size = glm::vec2(2.0f, 2.0f);
        registry.AddComponent<TransformComponent>(m_TriggerID, t);
        
        ColliderComponent c;
        c.SetAutoBounds(true);
        c.SetTrigger(true);
        registry.AddComponent<ColliderComponent>(m_TriggerID, c);
    }

    void Update(float dt) override {
        m_Physics.Update(dt);
        
        // On server: check if all active players are inside the trigger
        if (m_NetService->GetMode() == NetworkMode::Server) {
            auto players = m_NetControl->GetActivePlayerEntities();
            if (!players.empty()) {
                bool allIn = true;
                for (EntityID id : players) {
                    if (!m_Physics.IsColliding(id, m_TriggerID)) {
                        allIn = false;
                        break;
                    }
                }
                if (allIn && OnTransitionTriggered) OnTransitionTriggered();
            }
        }
    }
};
```

### Step D: Register in `Game`
Bind the callbacks inside your `Game` initialization (`OnInit()`) to trigger the SceneManager and broadcast transition commands:

```cpp
// mygame.cpp
bool MyGame::OnInit() {
    m_Level1 = new Level1(&m_AssetManager, ...);
    m_Level2 = new Level2(&m_AssetManager, ...);
    
    m_SceneManager.SetInitialScene(m_Level1);
    
    // Server transition trigger
    m_Level1->OnTransitionTriggered = [this]() {
        ServerCommandPacket packet;
        packet.header.type = PacketType::ServerCommand;
        packet.header.tick = m_NetControl->GetServerTick();
        strncpy(packet.command, "load_scene level2", sizeof(packet.command));
        m_NetService->BroadcastPacket(0, &packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);
        
        m_NetControl->OnSceneChanged(); // Reset state
        this->SetScene(m_Level2);       // Load Level 2
    };

    // Client transition receiver
    m_NetControl->OnServerCommandReceived = [this](const std::string& command) {
        if (command == "load_scene level2") {
            m_NetControl->OnSceneChanged(); // Reset state
            this->SetScene(m_Level2);       // Load Level 2
        }
    };
    return true;
}
```

---

## 3. Troubleshooting & Best Practices

* **Port Availability**: ENet will fail to bind if another instance of the server is running on port `7777`. Use `killall framework` on Linux or check task manager on Windows before hosting.
* **Large Snap Desyncs**: If clients snap continuously, verify that the `OnApplyInput()` physics changes match exactly on both server and client. Any discrepancy (e.g. framerate-dependent physics or local random variables) will cause prediction drift.
* **Kinematic Remote Proxies**: Ensure that remote players are Kinematic on clients. If they are Dynamic, client-side gravity will pull them down while the server tries to project them horizontally, causing constant desync snaps.
