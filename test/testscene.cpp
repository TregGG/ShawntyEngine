#include "testscene.h"
#define ENGINE_CLASS "TestScene"
#include "../core/enginedebug.h"
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#include "../assets/assetmanager.h"
#include "../core/input.h"
#include "../objects/gameobject.h"
#include "../objects/components/animator.h"
#include "../objects/components/spriterenderer2d.h"
#include "../objects/components/collidercomponent.h"
#include "../objects/components/rigidbodycomponent.h"
#include "../services/base/raycast.h"
#include "../services/networkservice.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include "testplayer.h"

void TestScene::OnEnter()
{
    registry.Init();
    m_Physics.Init();
    m_Physics.BindRegistry(&registry);

    if (!m_Assets)
    {
        ENGINE_ERROR("OnEnter failed: no AssetManager provided");
        return;
    }

    const SpriteSheetAsset* sheet = m_Assets->GetSpriteSheet("testobj");
    const AnimationSetAsset* animSet = m_Assets->GetAnimationSet("testobj");

    // 1. Player spawning removed until connected.

    // 2. Create Ground
    auto groundObj = std::make_unique<GameObject>(this, "Ground");
    EntityID gID = groundObj->GetID();

    TransformComponent gt;
    gt.position = glm::vec2(0.0f, -3.0f);
    gt.size = glm::vec2(15.0f, 1.0f);
    registry.AddComponent<TransformComponent>(gID, gt);

    SpriteComponent2D gs;
    gs.spriteSheet = sheet;
    gs.frameIndex = 0;
    gs.layer = Layer::Background;
    registry.AddComponent<SpriteComponent2D>(gID, gs);

    ColliderComponent gc;
    gc.SetAutoBounds(true);
    registry.AddComponent<ColliderComponent>(gID, gc);

    RigidBodyComponent grb;
    grb.SetType(BodyType::Static);
    grb.SetElasticity(0.0f);
    registry.AddComponent<RigidBodyComponent>(gID, grb);

    m_GameObjects.push_back(std::move(groundObj));

    // 3. Create Trampoline
    auto trampObj = std::make_unique<GameObject>(this, "Trampoline");
    EntityID tID = trampObj->GetID();

    TransformComponent tt;
    tt.position = glm::vec2(0.0f, -1.0f);
    tt.size = glm::vec2(3.0f, 0.5f);
    registry.AddComponent<TransformComponent>(tID, tt);

    SpriteComponent2D ts;
    ts.spriteSheet = sheet;
    ts.frameIndex = 0;
    ts.layer = Layer::Foreground;
    registry.AddComponent<SpriteComponent2D>(tID, ts);

    ColliderComponent tc;
    tc.SetAutoBounds(true);
    registry.AddComponent<ColliderComponent>(tID, tc);

    RigidBodyComponent trb;
    trb.SetType(BodyType::Static);
    trb.SetElasticity(2.5f); // Super bouncy!
    registry.AddComponent<RigidBodyComponent>(tID, trb);
    
    m_GameObjects.push_back(std::move(trampObj));

    // 4. Create UI
    bool isServer = (m_NetService && m_NetService->GetMode() == NetworkMode::Server);
    if (m_FontEngine && !isServer) {
        // UI Panel Background
        auto panel = std::make_unique<UIPanel>(this, "MainPanel");
        panel->Position = glm::vec2(50.0f, 50.0f);
        panel->Size = glm::vec2(300.0f, 250.0f);
        panel->BackgroundColor = glm::vec4(0.2f, 0.2f, 0.2f, 0.8f);

        // UI Text
        auto text = std::make_unique<UIText>(this, "TitleText", m_FontEngine);
        text->Position = glm::vec2(20.0f, 10.0f);
        text->Text = "Multiplayer Test";
        text->TextColor = glm::vec3(1.0f, 0.8f, 0.0f); // Gold
        panel->AddChild(std::move(text));

        // IP Input Field
        auto inputF = std::make_unique<UIInputField>(this, "IPInput", m_EventService, m_FontEngine);
        inputF->Position = glm::vec2(20.0f, 60.0f);
        inputF->Size = glm::vec2(250.0f, 40.0f);
        
        UIText* rawInputText = inputF->GetTextElement();
        rawInputText->Size = inputF->Size;
        rawInputText->HorizontalAlign = TextAlignment::Left;
        rawInputText->VerticalAlign = VerticalAlignment::Middle;
        rawInputText->Position = glm::vec2(5.0f, 0.0f);
        rawInputText->Text = "127.0.0.1"; // Default IP

        // Host Button
        auto hostBtn = std::make_unique<UIButton>(this, "HostButton", m_EventService);
        hostBtn->Position = glm::vec2(20.0f, 120.0f);
        hostBtn->Size = glm::vec2(120.0f, 40.0f);
        
        auto hostText = std::make_unique<UIText>(this, "HostBtnText", m_FontEngine);
        hostText->Position = glm::vec2(0.0f, 0.0f);
        hostText->Size = hostBtn->Size;
        hostText->HorizontalAlign = TextAlignment::Center;
        hostText->VerticalAlign = VerticalAlignment::Middle;
        hostText->Text = "Host";
        hostText->TextColor = glm::vec3(1.0f);
        hostBtn->AddChild(std::move(hostText));

        // Join Button
        auto joinBtn = std::make_unique<UIButton>(this, "JoinButton", m_EventService);
        joinBtn->Position = glm::vec2(150.0f, 120.0f);
        joinBtn->Size = glm::vec2(120.0f, 40.0f);
        
        auto joinText = std::make_unique<UIText>(this, "JoinBtnText", m_FontEngine);
        joinText->Position = glm::vec2(0.0f, 0.0f);
        joinText->Size = joinBtn->Size;
        joinText->HorizontalAlign = TextAlignment::Center;
        joinText->VerticalAlign = VerticalAlignment::Middle;
        joinText->Text = "Join";
        joinText->TextColor = glm::vec3(1.0f);
        joinBtn->AddChild(std::move(joinText));

        // Status Text
        auto statusTxt = std::make_unique<UIText>(this, "StatusText", m_FontEngine);
        statusTxt->Position = glm::vec2(20.0f, 180.0f);
        statusTxt->Text = "Offline";
        statusTxt->TextColor = glm::vec3(0.5f, 0.5f, 0.5f);
        m_StatusText = statusTxt.get();
        panel->AddChild(std::move(statusTxt));

        // Callbacks
        hostBtn->OnClickCallback = [this]() {
            if (m_NetService) {
                ENGINE_LOG("Host button clicked. Launching dedicated server...");
#ifdef _WIN32
                std::system("start bin\\framework.exe --server");
#elif defined(__APPLE__)
                std::system("open -n ./bin/framework --args --server");
#else
                std::system("./bin/framework --server &");
#endif
                m_NetService->Connect("127.0.0.1", 7777);
                if (m_StatusText) m_StatusText->Text = "Connecting...";
            }
        };

        joinBtn->OnClickCallback = [this, inputPtr = inputF.get()]() {
            if (m_NetService) {
                std::string ip = inputPtr->GetTextElement()->Text;
                if (ip.empty()) ip = "127.0.0.1";
                ENGINE_LOG("Join button clicked. Connecting to %s:7777", ip.c_str());
                m_NetService->Connect(ip, 7777);
                if (m_StatusText) m_StatusText->Text = "Connecting...";
            }
        };

        panel->AddChild(std::move(inputF));
        panel->AddChild(std::move(hostBtn));
        panel->AddChild(std::move(joinBtn));

        registry.AddUIElement(std::move(panel));
    }
    
    if (m_NetService) {
        m_NetService->SetPacketCallback([this](ENetPeer* peer, void* data, size_t size) {
            this->OnNetworkPacket(peer, data, size);
        });
        m_NetService->OnClientDisconnected = [this](ENetPeer* peer) {
            this->OnClientDisconnected(peer);
        };
    }

    // Set a reasonable view
    m_Camera.SetScale(2.0f);
    m_Camera.SetCameraPosition(0.0f, 0.0f);
}

void TestScene::OnExit()
{
    m_Physics.Shutdown();
    registry.Shutdown();
}

// void TestScene::SetInput(const Input& input)
// {
//     m_Input = &input;
// }

void TestScene::Update(float deltatime)
{
    // Query input if available
    glm::vec2 pushForce(0.0f);
    glm::vec2 cameraDir(0.0f);

    EntityID playerEntity = 0;
    if (m_GameObjects.size() > 0) playerEntity = m_GameObjects[0]->GetID();

    if (m_Input)
    {
        // Arrow keys for camera movement
        if (m_Input->IsKeyDown(GLFW_KEY_UP))    cameraDir.y += 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_DOWN))  cameraDir.y -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_LEFT))  cameraDir.x -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_RIGHT)) cameraDir.x += 1.0f;
    }

    // Pass input to player and update it
    if (!m_NetService || m_NetService->GetMode() != NetworkMode::Client) {
        if (!m_GameObjects.empty()) {
            if (auto* player = dynamic_cast<TestPlayer*>(m_GameObjects[0].get())) {
                player->PassInput(m_Input);
                player->Update(deltatime);
            }
        }
    }
    
    // Networking Updates
    if (m_NetService) {
        if (m_NetService->GetMode() == NetworkMode::Client) {
            if (m_StatusText && m_NetService->IsConnected()) {
                m_StatusText->Text = "Connected!";
                m_StatusText->TextColor = glm::vec3(0.0f, 1.0f, 0.0f);
            } else if (m_StatusText && !m_NetService->IsConnected()) {
                m_StatusText->Text = "Connecting...";
                m_StatusText->TextColor = glm::vec3(1.0f, 1.0f, 0.0f);
            }
            
            if (m_NetService->IsConnected() && !m_UIHidden) {
                registry.ClearUIElements();
                m_StatusText = nullptr;
                m_UIHidden = true;
            }
            
            // Interpolate remote player proxies towards their target server positions
            for (auto const& [localID, targetPos] : m_TargetPositions) {
                if (registry.HasComponent<TransformComponent>(localID)) {
                    auto& trans = registry.GetComponent<TransformComponent>(localID);
                    trans.position = glm::mix(trans.position, targetPos, std::min(15.0f * deltatime, 1.0f));
                }
            }
            
            if (m_Input && m_NetService->IsConnected()) {
                ClientInputPacket inputPacket;
                inputPacket.header.type = PacketType::ClientInput;
                inputPacket.header.tick = 0;
                inputPacket.inputMask = 0;
                if (m_Input->IsKeyDown(GLFW_KEY_W) || m_Input->IsKeyDown(GLFW_KEY_UP)) inputPacket.inputMask |= 1;
                if (m_Input->IsKeyDown(GLFW_KEY_S) || m_Input->IsKeyDown(GLFW_KEY_DOWN)) inputPacket.inputMask |= 2;
                if (m_Input->IsKeyDown(GLFW_KEY_A) || m_Input->IsKeyDown(GLFW_KEY_LEFT)) inputPacket.inputMask |= 4;
                if (m_Input->IsKeyDown(GLFW_KEY_D) || m_Input->IsKeyDown(GLFW_KEY_RIGHT)) inputPacket.inputMask |= 8;
                
                m_NetService->SendPacket(m_NetService->GetServerPeer(), 0, &inputPacket, sizeof(inputPacket), 0);
            }
        } else if (m_NetService->GetMode() == NetworkMode::Server) {
            std::vector<char> buffer(sizeof(PacketHeader) + sizeof(uint32_t) + m_PeerToEntity.size() * sizeof(EntityTransformData));
            
            PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer.data());
            header->type = PacketType::ServerUpdate;
            header->tick = 0;
            
            uint32_t* count = reinterpret_cast<uint32_t*>(buffer.data() + sizeof(PacketHeader));
            *count = m_PeerToEntity.size();
            
            EntityTransformData* transforms = reinterpret_cast<EntityTransformData*>(buffer.data() + sizeof(PacketHeader) + sizeof(uint32_t));
            
            int i = 0;
            for (auto const& [peer, entID] : m_PeerToEntity) {
                transforms[i].entityID = entID;
                if (registry.HasComponent<TransformComponent>(entID)) {
                    auto& t = registry.GetComponent<TransformComponent>(entID);
                    transforms[i].x = t.position.x;
                    transforms[i].y = t.position.y;
                } else {
                    transforms[i].x = 0; transforms[i].y = 0;
                }
                i++;
            }
            
            if (m_PeerToEntity.size() > 0) {
                m_NetService->BroadcastPacket(0, buffer.data(), buffer.size(), 0);
            }
        }
    }

    m_Physics.Update(deltatime);

    // Apply input-driven camera movement
    glm::vec2 camPos = m_Camera.GetCameraPosition();
    m_Camera.SetCameraPosition(
        camPos.x + cameraDir.x * m_MoveSpeed * deltatime,
        camPos.y + cameraDir.y * m_MoveSpeed * deltatime
    );

    // Animators Update
    for (EntityID e : registry.ViewAnimators()) {
        auto& animator = registry.GetComponent<AnimatorComponent>(e);
        if (animator.IsActive()) {
            animator.Update(deltatime);
        }
    }
    
    // Transform Hierarchy Update (rudimentary transform system)
    // Manual transform update loop removed! The engine computes this automatically on the fly via GetWorldPosition()
    // SHAPE CAST DEMONSTRATION
    m_TestLines.clear();
    m_TestRects.clear();
    if (playerEntity != 0 && registry.HasComponent<TransformComponent>(playerEntity)) {
        glm::vec2 pPos = registry.GetComponent<TransformComponent>(playerEntity).position;
        
        // Ignore UI layer AND Player layer so we don't immediately hit ourselves!
        uint32_t testingMask = ~( (1 << static_cast<int>(Layer::UI)) | (1 << static_cast<int>(Layer::Player)) ); 

        // 1. Box Cast to the right
        RaycastHit boxHit;
        glm::vec2 boxDir(1.0f, 0.0f);
        glm::vec2 boxSize(1.0f, 1.0f);
        float boxLen = 15.0f;
        
        if (BOX_CAST(pPos, pPos + boxDir * boxLen, boxSize, boxHit, testingMask)) {
            m_TestLines.push_back({pPos, boxHit.point, glm::vec3(1.0f, 0.5f, 0.0f)}); // Orange line
            m_TestRects.push_back({glm::vec3(boxHit.point, 0.0f), boxSize, glm::vec3(1.0f, 0.5f, 0.0f)}); // Orange box
        } else {
            m_TestLines.push_back({pPos, pPos + boxDir * boxLen, glm::vec3(0.0f, 1.0f, 0.0f)});
            m_TestRects.push_back({glm::vec3(pPos + boxDir * boxLen, 0.0f), boxSize, glm::vec3(0.0f, 1.0f, 0.0f)});
        }

        // 2. Circle Cast upwards
        RaycastHit circHit;
        glm::vec2 circDir(0.0f, 1.0f);
        float circRadius = 0.5f;
        float circLen = 10.0f;
        
        if (CIRCLE_CAST(pPos, pPos + circDir * circLen, circRadius, circHit, testingMask)) {
            m_TestLines.push_back({pPos, circHit.point, glm::vec3(0.0f, 0.5f, 1.0f)}); // Blue line
            m_TestRects.push_back({glm::vec3(circHit.point, 0.0f), glm::vec2(circRadius * 2.0f), glm::vec3(0.0f, 0.5f, 1.0f)}); // Blue square
        } else {
            m_TestLines.push_back({pPos, pPos + circDir * circLen, glm::vec3(0.0f, 1.0f, 0.0f)});
            m_TestRects.push_back({glm::vec3(pPos + circDir * circLen, 0.0f), glm::vec2(circRadius * 2.0f), glm::vec3(0.0f, 1.0f, 0.0f)});
        }
    }
}

void TestScene::BuildDebugRenderables(std::vector<DebugRect>& outDebugRects) const
{
#ifdef ENGINE_DEBUG
    outDebugRects.clear();
    for (EntityID e : registry.ViewPhysicsObjects()) 
    {
        const auto& col = registry.GetComponent<ColliderComponent>(e);
        const auto& trans = registry.GetComponent<TransformComponent>(e);
        
        auto b = col.GetBounds(trans);
        glm::vec2 size(b.maxX - b.minX, b.maxY - b.minY);
        glm::vec3 pos(b.minX + size.x * 0.5f, b.minY + size.y * 0.5f, 0.0f);
        
        // Render triggers as RED instead of yellow
        glm::vec3 cColor = col.IsTrigger() ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        outDebugRects.push_back({pos, size, cColor});
    }
    
    // Append the custom shape cast test rectangles
    for (const auto& r : m_TestRects) {
        outDebugRects.push_back(r);
    }
#endif
}

void TestScene::BuildDebugLines(std::vector<DebugLine>& outDebugLines) const
{
#ifdef ENGINE_DEBUG
    outDebugLines = m_TestLines;
#endif
}

void TestScene::OnNetworkPacket(ENetPeer* peer, void* data, size_t size) {
    if (!m_NetService || size < sizeof(PacketHeader)) return;
    PacketHeader* header = reinterpret_cast<PacketHeader*>(data);

    if (m_NetService->GetMode() == NetworkMode::Server) {
        if (header->type == PacketType::ClientInput && size >= sizeof(ClientInputPacket)) {
            if (m_PeerToEntity.find(peer) == m_PeerToEntity.end()) {
                // Spawn new player for peer
                auto playerObj = std::make_unique<TestPlayer>(this, "RemotePlayer", m_Assets->GetSpriteSheet("testobj"));
                EntityID pID = playerObj->GetID();
                // move player up slightly so they don't clip floor immediately
                registry.GetComponent<TransformComponent>(pID).position = glm::vec2(0.0f, 2.0f);
                m_GameObjects.push_back(std::move(playerObj));
                m_PeerToEntity[peer] = pID;
                ENGINE_LOG("Server spawned new player for peer!");

                // Send connection welcome packet back to peer with their unique Player Entity ID
                ConnectPacket welcomePacket;
                welcomePacket.header.type = PacketType::Connect;
                welcomePacket.header.tick = 0;
                welcomePacket.clientEntityID = pID;
                m_NetService->SendPacket(peer, 0, &welcomePacket, sizeof(welcomePacket), ENET_PACKET_FLAG_RELIABLE);
            }
            EntityID id = m_PeerToEntity[peer];
            ClientInputPacket* input = reinterpret_cast<ClientInputPacket*>(data);
            
            if (registry.HasComponent<RigidBodyComponent>(id)) {
                auto& rb = registry.GetComponent<RigidBodyComponent>(id);
                glm::vec2 vel(0.0f);
                if (input->inputMask & 1) vel.y += 1.0f; // W
                if (input->inputMask & 2) vel.y -= 1.0f; // S
                if (input->inputMask & 4) vel.x -= 1.0f; // A
                if (input->inputMask & 8) vel.x += 1.0f; // D
                vel *= m_MoveSpeed;
                
                glm::vec2 currentVel = rb.GetVelocity();
                rb.SetVelocity(glm::vec2(vel.x, currentVel.y)); // Keep gravity for Y
                if (input->inputMask & 1) { // If jump (W)
                     rb.AddForce(glm::vec2(0.0f, 10.0f)); // Simple jump impulse for fun
                }
            }
        }
    } else if (m_NetService->GetMode() == NetworkMode::Client) {
        if (header->type == PacketType::Connect && size >= sizeof(ConnectPacket)) {
            ConnectPacket* connPacket = reinterpret_cast<ConnectPacket*>(data);
            uint32_t myEntityID = connPacket->clientEntityID;
            ENGINE_LOG("[Client] Received connection confirmation! My Player Entity ID is: %u", myEntityID);
        } else if (header->type == PacketType::ServerUpdate) {
            size_t countOffset = sizeof(PacketHeader);
            if (size < countOffset + sizeof(uint32_t)) return;
            uint32_t count = *reinterpret_cast<uint32_t*>(static_cast<char*>(data) + countOffset);
            
            size_t dataOffset = countOffset + sizeof(uint32_t);
            if (size < dataOffset + count * sizeof(EntityTransformData)) return;
            EntityTransformData* transforms = reinterpret_cast<EntityTransformData*>(static_cast<char*>(data) + dataOffset);
            
            // Track all active server entity IDs in this update
            std::vector<uint32_t> activeServerIDs;
            activeServerIDs.reserve(count);
            
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t sID = transforms[i].entityID;
                activeServerIDs.push_back(sID);
                
                if (m_ServerToLocalEntity.find(sID) == m_ServerToLocalEntity.end()) {
                    // Spawn remote player proxy
                    auto playerObj = std::make_unique<TestPlayer>(this, "ProxyPlayer", m_Assets->GetSpriteSheet("testobj"));
                    // Strip its rigidbody so it doesn't simulate physics locally (server dictates position)
                    if (registry.HasComponent<RigidBodyComponent>(playerObj->GetID())) {
                        registry.RemoveComponent<RigidBodyComponent>(playerObj->GetID());
                    }
                    EntityID lID = playerObj->GetID();
                    m_GameObjects.push_back(std::move(playerObj));
                    m_ServerToLocalEntity[sID] = lID;
                    
                    // Snap position immediately on first receipt to avoid visual sliding from default position
                    if (registry.HasComponent<TransformComponent>(lID)) {
                        auto& trans = registry.GetComponent<TransformComponent>(lID);
                        trans.position = glm::vec2(transforms[i].x, transforms[i].y);
                    }
                }
                EntityID localID = m_ServerToLocalEntity[sID];
                m_TargetPositions[localID] = glm::vec2(transforms[i].x, transforms[i].y);
            }
            
            // Clean up players that have disconnected (no longer in server updates)
            std::vector<uint32_t> toRemove;
            for (auto const& [sID, lID] : m_ServerToLocalEntity) {
                if (std::find(activeServerIDs.begin(), activeServerIDs.end(), sID) == activeServerIDs.end()) {
                    toRemove.push_back(sID);
                }
            }
            
            for (uint32_t sID : toRemove) {
                EntityID lID = m_ServerToLocalEntity[sID];
                
                // Find and get weapon ID
                EntityID weaponID = 0;
                for (auto const& obj : m_GameObjects) {
                    if (obj->GetID() == lID) {
                        if (auto* player = dynamic_cast<TestPlayer*>(obj.get())) {
                            weaponID = player->GetWeaponID();
                        }
                        break;
                    }
                }
                
                // Remove from m_GameObjects
                auto it = std::remove_if(m_GameObjects.begin(), m_GameObjects.end(), [lID](const std::unique_ptr<GameObject>& obj) {
                    return obj->GetID() == lID;
                });
                if (it != m_GameObjects.end()) {
                    m_GameObjects.erase(it, m_GameObjects.end());
                }
                
                // Destroy entity and weapon from registry
                registry.Destroy(lID);
                if (weaponID != 0) {
                    registry.Destroy(weaponID);
                }
                
                m_ServerToLocalEntity.erase(sID);
                m_TargetPositions.erase(lID);
                ENGINE_LOG("Client cleaned up local proxy player (ID: %d) and weapon (ID: %d) for server entity %d", lID, weaponID, sID);
            }
        }
    }
}

void TestScene::OnClientDisconnected(ENetPeer* peer) {
    if (m_PeerToEntity.find(peer) != m_PeerToEntity.end()) {
        EntityID entID = m_PeerToEntity[peer];
        
        // Find player in game objects to get its weapon ID
        EntityID weaponID = 0;
        for (auto const& obj : m_GameObjects) {
            if (obj->GetID() == entID) {
                if (auto* player = dynamic_cast<TestPlayer*>(obj.get())) {
                    weaponID = player->GetWeaponID();
                }
                break;
            }
        }
        
        // 1. Remove from m_GameObjects
        auto it = std::remove_if(m_GameObjects.begin(), m_GameObjects.end(), [entID](const std::unique_ptr<GameObject>& obj) {
            return obj->GetID() == entID;
        });
        if (it != m_GameObjects.end()) {
            m_GameObjects.erase(it, m_GameObjects.end());
        }
        
        // 2. Destroy the player entity and weapon entity from registry
        registry.Destroy(entID);
        if (weaponID != 0) {
            registry.Destroy(weaponID);
        }
        
        // 3. Remove from maps
        m_PeerToEntity.erase(peer);
        ENGINE_LOG("Server cleaned up player entity (ID: %d) and weapon (ID: %d) for disconnected peer.", entID, weaponID);
    }
}
