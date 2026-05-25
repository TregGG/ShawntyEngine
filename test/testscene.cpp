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
    glm::vec2 cameraDir(0.0f);

    EntityID playerEntity = 0;
    if (m_NetService && m_NetService->GetMode() == NetworkMode::Client) {
        playerEntity = m_MyLocalPlayerID;
    } else if (!m_GameObjects.empty()) {
        playerEntity = m_GameObjects[0]->GetID();
    }

    if (m_Input)
    {
        // Arrow keys for camera movement
        if (m_Input->IsKeyDown(GLFW_KEY_UP))    cameraDir.y += 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_DOWN))  cameraDir.y -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_LEFT))  cameraDir.x -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_RIGHT)) cameraDir.x += 1.0f;
    }

    // Fixed 60Hz tick accumulator
    float tickInterval = 1.0f / 60.0f;
    m_TimeAccumulator += deltatime;
    while (m_TimeAccumulator >= tickInterval)
    {
        if (m_NetService) {
            if (m_NetService->GetMode() == NetworkMode::Server) {
                m_ServerTick++;
                SimulateServerTick();
                m_Physics.Update(tickInterval);

                // Broadcast ServerUpdate (velocities) or StateSync (positions every 15 ticks)
                if (m_ServerTick % 15 == 0) {
                    std::vector<char> buffer(sizeof(PacketHeader) + sizeof(uint32_t) + m_PeerToEntity.size() * sizeof(EntityPositionData));
                    
                    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer.data());
                    header->type = PacketType::StateSync;
                    header->tick = m_ServerTick;
                    
                    uint32_t* count = reinterpret_cast<uint32_t*>(buffer.data() + sizeof(PacketHeader));
                    *count = m_PeerToEntity.size();
                    
                    EntityPositionData* positions = reinterpret_cast<EntityPositionData*>(buffer.data() + sizeof(PacketHeader) + sizeof(uint32_t));
                    
                    int idx = 0;
                    for (auto const& [peer, entID] : m_PeerToEntity) {
                        positions[idx].entityID = entID;
                        if (registry.HasComponent<TransformComponent>(entID)) {
                            // Project the player's position 3 ticks ahead to compensate for packet cycles
                            glm::vec2 projectedPos = ProjectPlayerState(entID, peer, m_ServerTick, 3);
                            positions[idx].x = projectedPos.x;
                            positions[idx].y = projectedPos.y;
                        } else {
                            positions[idx].x = 0; positions[idx].y = 0;
                        }
                        idx++;
                    }
                    
                    if (m_PeerToEntity.size() > 0) {
                        m_NetService->BroadcastPacket(0, buffer.data(), buffer.size(), 0);
                    }
                } else {
                    std::vector<char> buffer(sizeof(PacketHeader) + sizeof(uint32_t) + m_PeerToEntity.size() * sizeof(EntityVelocityData));
                    
                    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer.data());
                    header->type = PacketType::ServerUpdate;
                    header->tick = m_ServerTick;
                    
                    uint32_t* count = reinterpret_cast<uint32_t*>(buffer.data() + sizeof(PacketHeader));
                    *count = m_PeerToEntity.size();
                    
                    EntityVelocityData* velocities = reinterpret_cast<EntityVelocityData*>(buffer.data() + sizeof(PacketHeader) + sizeof(uint32_t));
                    
                    int idx = 0;
                    for (auto const& [peer, entID] : m_PeerToEntity) {
                        velocities[idx].entityID = entID;
                        if (registry.HasComponent<RigidBodyComponent>(entID)) {
                            auto& rb = registry.GetComponent<RigidBodyComponent>(entID);
                            glm::vec2 vel = rb.GetVelocity();
                            velocities[idx].vx = vel.x;
                            velocities[idx].vy = vel.y;
                        } else {
                            velocities[idx].vx = 0.0f; velocities[idx].vy = 0.0f;
                        }
                        idx++;
                    }
                    
                    if (m_PeerToEntity.size() > 0) {
                        m_NetService->BroadcastPacket(0, buffer.data(), buffer.size(), 0);
                    }
                }
            } else if (m_NetService->GetMode() == NetworkMode::Client) {
                m_ClientTick++;
                uint16_t inputMask = 0;
                
                if (m_Input && m_NetService->IsConnected()) {
                    bool spaceDown = m_Input->IsKeyDown(GLFW_KEY_W) || m_Input->IsKeyDown(GLFW_KEY_UP) || m_Input->IsKeyDown(GLFW_KEY_SPACE);
                    if (spaceDown) {
                        if (!m_SpaceWasPressed) {
                            inputMask |= 1;
                        }
                        m_SpaceWasPressed = true;
                    } else {
                        m_SpaceWasPressed = false;
                    }
                    if (m_Input->IsKeyDown(GLFW_KEY_S) || m_Input->IsKeyDown(GLFW_KEY_DOWN)) inputMask |= 2;
                    if (m_Input->IsKeyDown(GLFW_KEY_A) || m_Input->IsKeyDown(GLFW_KEY_LEFT)) inputMask |= 4;
                    if (m_Input->IsKeyDown(GLFW_KEY_D) || m_Input->IsKeyDown(GLFW_KEY_RIGHT)) inputMask |= 8;
                }

                if (m_MyLocalPlayerID != 0) {
                    ApplyInput(m_MyLocalPlayerID, inputMask);
                    m_ClientInputHistory[m_ClientTick] = inputMask;
                }

                m_Physics.Update(tickInterval);

                if (m_MyLocalPlayerID != 0 && registry.HasComponent<TransformComponent>(m_MyLocalPlayerID)) {
                    auto& trans = registry.GetComponent<TransformComponent>(m_MyLocalPlayerID);
                    glm::vec2 vel(0.0f);
                    if (registry.HasComponent<RigidBodyComponent>(m_MyLocalPlayerID)) {
                        vel = registry.GetComponent<RigidBodyComponent>(m_MyLocalPlayerID).GetVelocity();
                    }
                    m_ClientStateHistory[m_ClientTick] = { trans.position, vel };
                }

                if (m_Input && m_NetService->IsConnected()) {
                    ClientInputPacket inputPacket;
                    inputPacket.header.type = PacketType::ClientInput;
                    inputPacket.header.tick = m_ClientTick;
                    inputPacket.inputMask = inputMask;
                    inputPacket.mouseDeltaX = 0.0f;
                    inputPacket.mouseDeltaY = 0.0f;
                    m_NetService->SendPacket(m_NetService->GetServerPeer(), 0, &inputPacket, sizeof(inputPacket), 0);
                }

                // Clean up history
                if (m_ClientTick > 120) {
                    uint32_t limit = m_ClientTick - 120;
                    auto it = m_ClientStateHistory.begin();
                    while (it != m_ClientStateHistory.end() && it->first < limit) {
                        it = m_ClientStateHistory.erase(it);
                    }
                    auto it2 = m_ClientInputHistory.begin();
                    while (it2 != m_ClientInputHistory.end() && it2->first < limit) {
                        it2 = m_ClientInputHistory.erase(it2);
                    }
                }
            }
        } else {
            m_Physics.Update(tickInterval);
        }

        m_TimeAccumulator -= tickInterval;
    }

    // Per-frame rendering updates
    if (m_NetService && m_NetService->GetMode() == NetworkMode::Client) {
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
        
    }

    // Camera movement
    glm::vec2 camPos = m_Camera.GetCameraPosition();
    m_Camera.SetCameraPosition(
        camPos.x + cameraDir.x * m_MoveSpeed * deltatime,
        camPos.y + cameraDir.y * m_MoveSpeed * deltatime
    );

    // Animators
    for (EntityID e : registry.ViewAnimators()) {
        auto& animator = registry.GetComponent<AnimatorComponent>(e);
        if (animator.IsActive()) {
            animator.Update(deltatime);
        }
    }

    // Shape cast demonstration
    m_TestLines.clear();
    m_TestRects.clear();
    if (playerEntity != 0 && registry.HasComponent<TransformComponent>(playerEntity)) {
        glm::vec2 pPos = registry.GetComponent<TransformComponent>(playerEntity).position;
        uint32_t testingMask = ~( (1 << static_cast<int>(Layer::UI)) | (1 << static_cast<int>(Layer::Player)) ); 

        RaycastHit boxHit;
        glm::vec2 boxDir(1.0f, 0.0f);
        glm::vec2 boxSize(1.0f, 1.0f);
        float boxLen = 15.0f;
        
        if (BOX_CAST(pPos, pPos + boxDir * boxLen, boxSize, boxHit, testingMask)) {
            m_TestLines.push_back({pPos, boxHit.point, glm::vec3(1.0f, 0.5f, 0.0f)});
            m_TestRects.push_back({glm::vec3(boxHit.point, 0.0f), boxSize, glm::vec3(1.0f, 0.5f, 0.0f)});
        } else {
            m_TestLines.push_back({pPos, pPos + boxDir * boxLen, glm::vec3(0.0f, 1.0f, 0.0f)});
            m_TestRects.push_back({glm::vec3(pPos + boxDir * boxLen, 0.0f), boxSize, glm::vec3(0.0f, 1.0f, 0.0f)});
        }

        RaycastHit circHit;
        glm::vec2 circDir(0.0f, 1.0f);
        float circRadius = 0.5f;
        float circLen = 10.0f;
        
        if (CIRCLE_CAST(pPos, pPos + circDir * circLen, circRadius, circHit, testingMask)) {
            m_TestLines.push_back({pPos, circHit.point, glm::vec3(0.0f, 0.5f, 1.0f)});
            m_TestRects.push_back({glm::vec3(circHit.point, 0.0f), glm::vec2(circRadius * 2.0f), glm::vec3(0.0f, 0.5f, 1.0f)});
        } else {
            m_TestLines.push_back({pPos, pPos + circDir * circLen, glm::vec3(0.0f, 1.0f, 0.0f)});
            m_TestRects.push_back({glm::vec3(pPos + circDir * circLen, 0.0f), glm::vec2(circRadius * 2.0f), glm::vec3(0.0f, 1.0f, 0.0f)});
        }
    }
    registry.Update(deltatime);
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

void TestScene::ApplyInput(EntityID entID, uint16_t inputMask) {
    if (registry.HasComponent<RigidBodyComponent>(entID)) {
        auto& rb = registry.GetComponent<RigidBodyComponent>(entID);
        glm::vec2 vel(0.0f);
        if (inputMask & 4) vel.x -= 1.0f; // A / Left
        if (inputMask & 8) vel.x += 1.0f; // D / Right
        vel *= m_MoveSpeed;
        
        glm::vec2 currentVel = rb.GetVelocity();
        rb.SetVelocity(glm::vec2(vel.x, currentVel.y)); // Keep gravity for Y
        if (inputMask & 1) { // Jump (W / Up / Space)
            rb.SetVelocity(glm::vec2(rb.GetVelocity().x, 18.0f));
        }
    }
}

void TestScene::SimulateServerTick() {
    for (auto const& [peer, entID] : m_PeerToEntity) {
        uint16_t inputMask = 0;
        
        auto peerInputsIt = m_BufferedPeerInputs.find(peer);
        if (peerInputsIt != m_BufferedPeerInputs.end()) {
            auto inputIt = peerInputsIt->second.find(m_ServerTick);
            if (inputIt != peerInputsIt->second.end()) {
                inputMask = inputIt->second;
                m_LastExecutedInputs[peer] = inputMask;
                peerInputsIt->second.erase(inputIt);
            } else {
                inputMask = m_LastExecutedInputs[peer];
            }
        } else {
            inputMask = m_LastExecutedInputs[peer];
        }
        
        ApplyInput(entID, inputMask);
    }
    
    // Clean up inputs older than m_ServerTick
    for (auto& [peer, tickMap] : m_BufferedPeerInputs) {
        auto it = tickMap.begin();
        while (it != tickMap.end() && it->first < m_ServerTick) {
            it = tickMap.erase(it);
        }
    }
}

glm::vec2 TestScene::ProjectPlayerState(EntityID entID, ENetPeer* peer, uint32_t startTick, int numTicks) {
    if (!registry.HasComponent<TransformComponent>(entID) || !registry.HasComponent<RigidBodyComponent>(entID)) {
        return glm::vec2(0.0f);
    }
    glm::vec2 pos = registry.GetComponent<TransformComponent>(entID).position;
    glm::vec2 vel = registry.GetComponent<RigidBodyComponent>(entID).GetVelocity();
    
    uint16_t lastInput = m_LastExecutedInputs[peer];
    float dt = 1.0f / 60.0f;
    
    for (int step = 1; step <= numTicks; ++step) {
        uint32_t targetTick = startTick + step;
        uint16_t inputMask = lastInput;
        
        auto peerInputsIt = m_BufferedPeerInputs.find(peer);
        if (peerInputsIt != m_BufferedPeerInputs.end()) {
            auto inputIt = peerInputsIt->second.find(targetTick);
            if (inputIt != peerInputsIt->second.end()) {
                inputMask = inputIt->second;
                lastInput = inputMask;
            }
        }
        
        glm::vec2 moveVel(0.0f);
        if (inputMask & 4) moveVel.x -= 1.0f;
        if (inputMask & 8) moveVel.x += 1.0f;
        moveVel *= m_MoveSpeed;
        
        vel.x = moveVel.x;
        vel.y += -9.81f * 2.0f * dt;
        vel *= (1.0f - 2.0f * dt);
        if (inputMask & 1) {
            vel.y = 18.0f;
        }
        pos += vel * dt;
    }
    
    // Safety clamp to prevent falling below ground pivot
    pos.y = std::max(pos.y, -2.0f);
    
    return pos;
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
                registry.GetComponent<TransformComponent>(pID).position = glm::vec2(0.0f, 2.0f);
                m_GameObjects.push_back(std::move(playerObj));
                m_PeerToEntity[peer] = pID;
                ENGINE_LOG("Server spawned new player for peer!");

                // Send connection welcome packet
                ConnectPacket welcomePacket;
                welcomePacket.header.type = PacketType::Connect;
                welcomePacket.header.tick = m_ServerTick; // server tick
                welcomePacket.clientEntityID = pID;
                m_NetService->SendPacket(peer, 0, &welcomePacket, sizeof(welcomePacket), ENET_PACKET_FLAG_RELIABLE);
            }
            ClientInputPacket* input = reinterpret_cast<ClientInputPacket*>(data);
            uint32_t clientTick = input->header.tick;
            uint16_t inputMask = input->inputMask;
            
            // Buffer inputs on server
            m_BufferedPeerInputs[peer][clientTick] = inputMask;
        }
    } else if (m_NetService->GetMode() == NetworkMode::Client) {
        if (header->type == PacketType::Connect && size >= sizeof(ConnectPacket)) {
            ConnectPacket* connPacket = reinterpret_cast<ConnectPacket*>(data);
            uint32_t myEntityID = connPacket->clientEntityID;
            m_MyServerPlayerID = myEntityID;
            
            // Synchronize client clock
            uint32_t serverTick = connPacket->header.tick;
            uint32_t rttTicks = 0;
            if (m_NetService->GetServerPeer()) {
                rttTicks = (m_NetService->GetServerPeer()->roundTripTime / 2) / 16.67f;
            }
            m_LatencyOffsetTicks = rttTicks + 5;
            m_ClientTick = serverTick + m_LatencyOffsetTicks;
            
            ENGINE_LOG("[Client] Received connection confirmation! My Player Entity ID is: %u, Server Tick: %u, RTT: %u ms (Ticks: %u), Initial Client Tick: %u", 
                myEntityID, serverTick, 
                m_NetService->GetServerPeer() ? m_NetService->GetServerPeer()->roundTripTime : 0,
                rttTicks, m_ClientTick);
        } else if (header->type == PacketType::ServerUpdate) {
            // Receive velocity update
            size_t countOffset = sizeof(PacketHeader);
            if (size < countOffset + sizeof(uint32_t)) return;
            uint32_t count = *reinterpret_cast<uint32_t*>(static_cast<char*>(data) + countOffset);
            
            size_t dataOffset = countOffset + sizeof(uint32_t);
            if (size < dataOffset + count * sizeof(EntityVelocityData)) return;
            EntityVelocityData* velocities = reinterpret_cast<EntityVelocityData*>(static_cast<char*>(data) + dataOffset);
            
            std::vector<uint32_t> activeServerIDs;
            activeServerIDs.reserve(count);
            
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t sID = velocities[i].entityID;
                activeServerIDs.push_back(sID);
                
                if (m_ServerToLocalEntity.find(sID) == m_ServerToLocalEntity.end()) {
                    auto playerObj = std::make_unique<TestPlayer>(this, "ProxyPlayer", m_Assets->GetSpriteSheet("testobj"));
                    EntityID lID = playerObj->GetID();
                    
                    if (sID == m_MyServerPlayerID) {
                        m_MyLocalPlayerID = lID;
                        ENGINE_LOG("[Client] Identified my local player entity (ID: %d)", lID);
                    } else {
                        // Set remote proxy player to kinematic with no gravity/drag
                        if (registry.HasComponent<RigidBodyComponent>(lID)) {
                            auto& rb = registry.GetComponent<RigidBodyComponent>(lID);
                            rb.SetType(BodyType::Kinematic);
                            rb.SetUseGravity(false);
                            rb.SetDrag(0.0f);
                        }
                    }
                    
                    m_GameObjects.push_back(std::move(playerObj));
                    m_ServerToLocalEntity[sID] = lID;
                    
                    if (registry.HasComponent<TransformComponent>(lID)) {
                        auto& trans = registry.GetComponent<TransformComponent>(lID);
                        trans.position = glm::vec2(0.0f, 2.0f); // Default spawn position
                    }
                }
                
                EntityID localID = m_ServerToLocalEntity[sID];
                if (localID != m_MyLocalPlayerID) {
                    if (registry.HasComponent<RigidBodyComponent>(localID)) {
                        auto& rb = registry.GetComponent<RigidBodyComponent>(localID);
                        rb.SetVelocity(glm::vec2(velocities[i].vx, velocities[i].vy));
                    }
                }
            }
            
            // Clean up players that have disconnected
            std::vector<uint32_t> toRemove;
            for (auto const& [sID, lID] : m_ServerToLocalEntity) {
                if (std::find(activeServerIDs.begin(), activeServerIDs.end(), sID) == activeServerIDs.end()) {
                    toRemove.push_back(sID);
                }
            }
            
            for (uint32_t sID : toRemove) {
                EntityID lID = m_ServerToLocalEntity[sID];
                EntityID weaponID = 0;
                for (auto const& obj : m_GameObjects) {
                    if (obj->GetID() == lID) {
                        if (auto* player = dynamic_cast<TestPlayer*>(obj.get())) {
                            weaponID = player->GetWeaponID();
                        }
                        break;
                    }
                }
                
                auto it = std::remove_if(m_GameObjects.begin(), m_GameObjects.end(), [lID](const std::unique_ptr<GameObject>& obj) {
                    return obj->GetID() == lID;
                });
                if (it != m_GameObjects.end()) {
                    m_GameObjects.erase(it, m_GameObjects.end());
                }
                
                registry.Destroy(lID);
                if (weaponID != 0) {
                    registry.Destroy(weaponID);
                }
                
                m_ServerToLocalEntity.erase(sID);
                ENGINE_LOG("Client cleaned up local proxy player (ID: %d) and weapon (ID: %d) for server entity %d", lID, weaponID, sID);
            }
        } else if (header->type == PacketType::StateSync) {
            // Receive absolute position sync (verification package)
            size_t countOffset = sizeof(PacketHeader);
            if (size < countOffset + sizeof(uint32_t)) return;
            uint32_t count = *reinterpret_cast<uint32_t*>(static_cast<char*>(data) + countOffset);
            
            size_t dataOffset = countOffset + sizeof(uint32_t);
            if (size < dataOffset + count * sizeof(EntityPositionData)) return;
            EntityPositionData* positions = reinterpret_cast<EntityPositionData*>(static_cast<char*>(data) + dataOffset);
            
            uint32_t serverUpdateTick = header->tick;
            
            std::vector<uint32_t> activeServerIDs;
            activeServerIDs.reserve(count);
            
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t sID = positions[i].entityID;
                activeServerIDs.push_back(sID);
                
                if (m_ServerToLocalEntity.find(sID) == m_ServerToLocalEntity.end()) {
                    auto playerObj = std::make_unique<TestPlayer>(this, "ProxyPlayer", m_Assets->GetSpriteSheet("testobj"));
                    EntityID lID = playerObj->GetID();
                    
                    if (sID == m_MyServerPlayerID) {
                        m_MyLocalPlayerID = lID;
                        ENGINE_LOG("[Client] Identified my local player entity (ID: %d)", lID);
                    } else {
                        // Set remote proxy player to kinematic with no gravity/drag
                        if (registry.HasComponent<RigidBodyComponent>(lID)) {
                            auto& rb = registry.GetComponent<RigidBodyComponent>(lID);
                            rb.SetType(BodyType::Kinematic);
                            rb.SetUseGravity(false);
                            rb.SetDrag(0.0f);
                        }
                    }
                    
                    m_GameObjects.push_back(std::move(playerObj));
                    m_ServerToLocalEntity[sID] = lID;
                    
                    if (registry.HasComponent<TransformComponent>(lID)) {
                        auto& trans = registry.GetComponent<TransformComponent>(lID);
                        trans.position = glm::vec2(positions[i].x, positions[i].y);
                    }
                }
                
                EntityID localID = m_ServerToLocalEntity[sID];
                glm::vec2 serverPos(positions[i].x, positions[i].y);
                
                if (localID == m_MyLocalPlayerID) {
                    if (registry.HasComponent<TransformComponent>(localID)) {
                        auto& trans = registry.GetComponent<TransformComponent>(localID);
                        
                        // Server update is projected 3 ticks ahead of serverUpdateTick
                        uint32_t reconTick = serverUpdateTick + 3;
                        auto historyIt = m_ClientStateHistory.find(reconTick);
                        if (historyIt != m_ClientStateHistory.end()) {
                            glm::vec2 predictedPos = historyIt->second.position;
                            glm::vec2 error = serverPos - predictedPos;
                            float errorLen = glm::length(error);
                            
                            if (errorLen > 1.5f) {
                                trans.position = serverPos;
                                if (registry.HasComponent<RigidBodyComponent>(localID)) {
                                    registry.GetComponent<RigidBodyComponent>(localID).SetVelocity(glm::vec2(0.0f));
                                }
                                ENGINE_LOG("[Client] Large desync detected (%.2f units) at tick %u. Snapping.", errorLen, reconTick);
                                m_ClientStateHistory.clear();
                            } else if (errorLen > 0.001f) {
                                glm::vec2 correction = error * 0.5f;
                                trans.position += correction;
                                
                                for (auto& [tick, state] : m_ClientStateHistory) {
                                    if (tick > reconTick) {
                                        state.position += correction;
                                    }
                                }
                            }
                        } else {
                            float dist = glm::distance(trans.position, serverPos);
                            if (dist > 1.5f) {
                                trans.position = serverPos;
                                if (registry.HasComponent<RigidBodyComponent>(localID)) {
                                    registry.GetComponent<RigidBodyComponent>(localID).SetVelocity(glm::vec2(0.0f));
                                }
                                ENGINE_LOG("[Client] No history for tick %u and large desync (%.2f units). Snapping.", reconTick, dist);
                            } else {
                                trans.position = glm::mix(trans.position, serverPos, 0.1f);
                            }
                        }
                    }
                } else {
                    // Soft position reconciliation for remote players (proxies)
                    if (registry.HasComponent<TransformComponent>(localID)) {
                        auto& trans = registry.GetComponent<TransformComponent>(localID);
                        float dist = glm::distance(trans.position, serverPos);
                        if (dist > 2.0f) {
                            trans.position = serverPos;
                        } else {
                            trans.position = glm::mix(trans.position, serverPos, 0.15f);
                        }
                    }
                }
            }
            
            // Clean up players that have disconnected
            std::vector<uint32_t> toRemove;
            for (auto const& [sID, lID] : m_ServerToLocalEntity) {
                if (std::find(activeServerIDs.begin(), activeServerIDs.end(), sID) == activeServerIDs.end()) {
                    toRemove.push_back(sID);
                }
            }
            
            for (uint32_t sID : toRemove) {
                EntityID lID = m_ServerToLocalEntity[sID];
                EntityID weaponID = 0;
                for (auto const& obj : m_GameObjects) {
                    if (obj->GetID() == lID) {
                        if (auto* player = dynamic_cast<TestPlayer*>(obj.get())) {
                            weaponID = player->GetWeaponID();
                        }
                        break;
                    }
                }
                
                auto it = std::remove_if(m_GameObjects.begin(), m_GameObjects.end(), [lID](const std::unique_ptr<GameObject>& obj) {
                    return obj->GetID() == lID;
                });
                if (it != m_GameObjects.end()) {
                    m_GameObjects.erase(it, m_GameObjects.end());
                }
                
                registry.Destroy(lID);
                if (weaponID != 0) {
                    registry.Destroy(weaponID);
                }
                
                m_ServerToLocalEntity.erase(sID);
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
