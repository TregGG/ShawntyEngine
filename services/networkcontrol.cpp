#include "networkcontrol.h"
#include "networkservice.h"
#include "contentsync.h"
#include "../core/projectpaths.h"
#include "../assets/assetmanager.h"
#include "../levels/scene.h"
#include "../levels/datadrivenscene.h"
#include "../core/input.h"
#include "../core/logger.h"
#include "../objects/components/components.h"
#include "../objects/components/rigidbodycomponent.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

#define ENGINE_CLASS "NetworkControl"
#include "../core/enginedebug.h"

NetworkControl::NetworkControl() {}
NetworkControl::~NetworkControl() {}

void NetworkControl::Init() {
    ENGINE_LOG("NetworkControl Initialized");
}

void NetworkControl::SetNetworkService(NetworkService* netService) {
    m_NetService = netService;
    if (m_NetService) {
        m_NetService->SetPacketCallback([this](ENetPeer* peer, void* data, size_t size) {
            this->OnPacketReceived(peer, data, size);
        });
        m_NetService->OnClientDisconnected = [this](ENetPeer* peer) {
            this->OnClientDisconnected(peer);
            if (m_ContentServer) m_ContentServer->OnPeerDisconnected(peer);
        };
        m_NetService->OnClientConnected = [this](ENetPeer* peer) {
            if (m_ContentServer) m_ContentServer->OnPeerConnected(peer);
        };
    }
}

void NetworkControl::Update(float dt) {
    if (m_NetService && m_NetService->GetMode() == NetworkMode::Client && m_Scene) {
        for (auto& [netId, remote] : m_Remote) {
            if (!m_Scene->registry.HasComponent<TransformComponent>(remote.entity)) continue;
            auto& trans = m_Scene->registry.GetComponent<TransformComponent>(remote.entity);
            glm::vec2& position = trans.parentTransform ? trans.localPosition : trans.position;
            if (!remote.body) position += remote.velocity * dt;

            auto it = m_VelocityCorrections.find(remote.entity);
            if (it != m_VelocityCorrections.end()) {
                // Apply the correction velocity to the physical position
                position += it->second * dt;

                // Decay the correction so it smoothly stops
                it->second *= std::exp(-5.0f * dt);
                if (glm::length(it->second) < 0.01f) m_VelocityCorrections.erase(it);
            }
        }
    }
}

void NetworkControl::Tick(float tickInterval) {
    if (!m_NetService || !m_Scene) return;

    if (m_NetService->GetMode() == NetworkMode::Server) {
        m_ServerTick++;
        if (m_ContentServer) m_ContentServer->Update(*m_NetService);
        
        // 1. Process client inputs (SimulateServerTick logic)
        for (auto const& [peer, entID] : m_PeerToEntity) {
            uint16_t lastInput = m_LastExecutedInputs[peer];
            auto peerInputsIt = m_BufferedPeerInputs.find(peer);
            
            if (peerInputsIt != m_BufferedPeerInputs.end()) {
                auto inputIt = peerInputsIt->second.find(m_ServerTick);
                if (inputIt != peerInputsIt->second.end()) {
                    lastInput = inputIt->second;
                    m_LastExecutedInputs[peer] = lastInput;
                    ++m_InputsUsed;
                } else {
                    ++m_InputsMissing;
                }
            }
            
            OnApplyInput(entID, lastInput);
            
            // Garbage collect old inputs
            if (peerInputsIt != m_BufferedPeerInputs.end()) {
                for (auto it = peerInputsIt->second.begin(); it != peerInputsIt->second.end(); ) {
                    if (it->first + 100 < m_ServerTick) {
                        it = peerInputsIt->second.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        
        if (s_PrintStats && m_ServerTick % 120 == 0 && m_InputsUsed + m_InputsMissing > 0) {
            std::fprintf(stderr, "inputs used=%u missing=%u (%.1f%%)\n", m_InputsUsed, m_InputsMissing,
                         100.0 * m_InputsMissing / (m_InputsUsed + m_InputsMissing));
            m_InputsUsed = m_InputsMissing = 0;
        }

        // 2. Broadcast TickSync
        if (m_ServerTick % 60 == 0) {
            PacketHeader tickHeader;
            tickHeader.type = PacketType::TickSync;
            tickHeader.tick = m_ServerTick;
            SendToPlayers(&tickHeader, sizeof(PacketHeader), ENET_PACKET_FLAG_RELIABLE);
        }

        // 3. Entity state
        EnsureReplicationScene();
        std::map<ENetPeer*, glm::vec2> own;
        for (auto const& [peer, entID] : m_PeerToEntity) own[peer] = ProjectPlayerState(entID, peer, m_ServerTick, 3);
        m_ReplicationServer.Tick(*m_NetService, m_ServerTick, own);

    } else if (m_NetService->GetMode() == NetworkMode::Client) {
        if (m_ContentClient && !m_ContentClient->IsReady()) return; // still downloading

        const uint16_t inputMask = m_Input ? OnGenerateInputMask(m_Input) : 0;
        for (int i = m_Clock.TicksToRun(); i > 0; --i) RunClientTick(inputMask);
    }
}

void NetworkControl::RunClientTick(uint16_t inputMask) {
    m_Clock.Advance();
    const uint32_t tick = m_Clock.GetTick();

    for (int i = ClientInputPacket::kInputs - 1; i > 0; --i) m_RecentInputs[i] = m_RecentInputs[i - 1];
    m_RecentInputs[0] = inputMask;
    SendClientInput(inputMask, 0.0f, 0.0f);
    m_ClientInputHistory[tick] = inputMask;

    // Clean history
    for (auto it = m_ClientInputHistory.begin(); it != m_ClientInputHistory.end();) {
        if (it->first + 100 < tick) it = m_ClientInputHistory.erase(it);
        else ++it;
    }
    for (auto it = m_ClientStateHistory.begin(); it != m_ClientStateHistory.end();) {
        if (it->first + 100 < tick) it = m_ClientStateHistory.erase(it);
        else ++it;
    }

    // Apply input locally for prediction
    if (m_MyLocalPlayerID != 0) {
        OnApplyInput(m_MyLocalPlayerID, inputMask);

        if (m_Scene->registry.HasComponent<TransformComponent>(m_MyLocalPlayerID) && m_Scene->registry.HasComponent<RigidBodyComponent>(m_MyLocalPlayerID)) {
            auto& trans = m_Scene->registry.GetComponent<TransformComponent>(m_MyLocalPlayerID);
            auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(m_MyLocalPlayerID);
            m_ClientStateHistory[tick] = { trans.position, rb.GetVelocity() };
        }
    }
}

void NetworkControl::SyncClock(uint32_t serverTick) {
    if (m_Clock.OnServerTick(serverTick, m_NetService->GetServerPeer())) {
        m_ClientStateHistory.clear();
        m_ClientInputHistory.clear();
        ENGINE_LOG("[Client] Clock resynchronized to tick %u.", m_Clock.GetTick());
    }
}

// Only players in the game: not people still joining, or without the join code
void NetworkControl::SendToPlayers(const void* data, size_t size, enet_uint32 flags) {
    m_Receivers.clear();
    for (auto const& [peer, entID] : m_PeerToEntity) m_Receivers.push_back(peer);
    m_NetService->Multicast(m_Receivers, 0, data, size, flags);
}

void NetworkControl::SendClientInput(uint16_t inputMask, float mouseX, float mouseY) {
    if (!m_NetService) return;
    ClientInputPacket packet;
    packet.header.type = PacketType::ClientInput;
    (void)inputMask; // already the newest entry in m_RecentInputs
    packet.header.tick = m_Clock.GetTick();
    std::memcpy(packet.inputMasks, m_RecentInputs, sizeof(packet.inputMasks));
    (void)mouseX;
    (void)mouseY;
    packet.epoch = m_Replication.GetEpoch();
    packet.ackSeq = m_Replication.GetAckSeq();
    packet.ackBits = m_Replication.GetAckBits();
    m_NetService->SendPacket(m_NetService->GetServerPeer(), 0, &packet, sizeof(packet), 0);
}

glm::vec2 NetworkControl::ProjectPlayerState(EntityID entID, ENetPeer* peer, uint32_t startTick, int numTicks) {
    if (!m_Scene || !m_Scene->registry.HasComponent<TransformComponent>(entID) || !m_Scene->registry.HasComponent<RigidBodyComponent>(entID)) {
        return glm::vec2(0.0f);
    }
    glm::vec2 pos = m_Scene->registry.GetComponent<TransformComponent>(entID).position;
    glm::vec2 vel = m_Scene->registry.GetComponent<RigidBodyComponent>(entID).GetVelocity();
    
    // Default projection doesn't know input application rules precisely unless overridden,
    // but a child could override this.
    return pos + vel * (numTicks / 60.0f);
}

void NetworkControl::Shutdown() {}

void NetworkControl::ProcessCommandString(const std::string& command) {}

void NetworkControl::OnPacketReceived(ENetPeer* peer, void* data, size_t size) {
    if (!m_NetService || size < sizeof(PacketHeader)) return;
    PacketHeader* header = reinterpret_cast<PacketHeader*>(data);

    if (m_NetService->GetMode() == NetworkMode::Server && m_ContentServer) {
        if (m_ContentServer->OnPacket(*m_NetService, peer, data, size)) return;
        if (!m_ContentServer->IsReady(peer)) return; // not joined, or still on an older version
    } else if (m_NetService->GetMode() == NetworkMode::Client && m_ContentClient) {
        if (m_ContentClient->OnPacket(*m_NetService, data, size)) return;
        if (!m_ContentClient->IsReady()) return;
    }

    if (m_NetService->GetMode() == NetworkMode::Server) {
        if (header->type == PacketType::ClientInput && size >= sizeof(ClientInputPacket)) {
            if (m_PeerToEntity.find(peer) == m_PeerToEntity.end()) {
                EnsureReplicationScene();
                EntityID pID = OnSpawnPlayer(peer, false);
                m_PeerToEntity[peer] = pID;

                const auto remembered = m_RememberedPositions.find(peer);
                if (remembered != m_RememberedPositions.end()) {
                    if (m_Scene && m_Scene->registry.HasComponent<TransformComponent>(pID))
                        m_Scene->registry.GetComponent<TransformComponent>(pID).position = remembered->second;
                    m_RememberedPositions.erase(remembered);
                }

                ConnectPacket welcomePacket;
                welcomePacket.header.type = PacketType::Connect;
                welcomePacket.header.tick = m_ServerTick;
                welcomePacket.clientEntityID = m_ReplicationServer.RegisterPlayer(*m_NetService, pID, m_ServerTick);
                welcomePacket.sceneEntityCount = m_ReplicationServer.GetSceneEntityCount();
                welcomePacket.epoch = m_ReplicationServer.GetEpoch();
                
                std::string scenePath = "";
                if (m_Scene && dynamic_cast<DataDrivenScene*>(m_Scene)) {
                    scenePath = ProjectPaths::MakeRelative(dynamic_cast<DataDrivenScene*>(m_Scene)->GetSceneFilePath());
                }
                strncpy(welcomePacket.sceneName, scenePath.c_str(), sizeof(welcomePacket.sceneName));
                welcomePacket.sceneName[sizeof(welcomePacket.sceneName) - 1] = '\0';
                
                m_NetService->SendPacket(peer, 0, &welcomePacket, sizeof(welcomePacket), ENET_PACKET_FLAG_RELIABLE);
                m_ReplicationServer.AddPeer(*m_NetService, peer, pID, m_ServerTick);
            }
            
            ClientInputPacket* inputPacket = reinterpret_cast<ClientInputPacket*>(data);
            m_ReplicationServer.OnAck(peer, inputPacket->epoch, inputPacket->ackSeq, inputPacket->ackBits);
            uint32_t targetTick = inputPacket->header.tick;
            auto& buffered = m_BufferedPeerInputs[peer];
            for (int i = 0; i < ClientInputPacket::kInputs; ++i) {
                const uint32_t tick = targetTick - static_cast<uint32_t>(i);
                // Too late to use, or further ahead than any real connection needs
                if (tick <= m_ServerTick || tick > m_ServerTick + 240) continue;
                uint16_t mask;
                std::memcpy(&mask, &inputPacket->inputMasks[i], sizeof(mask));
                buffered.emplace(tick, mask); // the first copy to arrive counts
            }
        }
    } else if (m_NetService->GetMode() == NetworkMode::Client) {
        if (header->type == PacketType::Connect && size >= sizeof(ConnectPacket)) {
            ConnectPacket* connectPacket = reinterpret_cast<ConnectPacket*>(data);
            
            // Clean up old state completely, because this is a fresh connection or scene transition
            m_ServerTick = 0;
            m_MyLocalPlayerID = 0;
            m_PeerToEntity.clear();
            m_Remote.clear();
            m_BufferedPeerInputs.clear();
            m_LastExecutedInputs.clear();
            m_ClientStateHistory.clear();
            m_ClientInputHistory.clear();

            m_MyServerPlayerID = connectPacket->clientEntityID;
            m_PendingEpoch = connectPacket->epoch;
            m_PendingSceneCount = connectPacket->sceneEntityCount;
            
            uint32_t serverTick = connectPacket->header.tick;
            m_Clock.Start(serverTick, m_NetService->GetServerPeer());
            std::fill(std::begin(m_RecentInputs), std::end(m_RecentInputs), 0);

            connectPacket->sceneName[sizeof(connectPacket->sceneName) - 1] = '\0';
            std::string sceneName(connectPacket->sceneName);
            if (OnClientConnectedCallback) {
                OnClientConnectedCallback(sceneName);
            } else {
                SpawnLocalPlayer();
            }
        } else if (header->type == PacketType::ServerCommand && size >= sizeof(ServerCommandPacket)) {
            ServerCommandPacket* cmdPacket = reinterpret_cast<ServerCommandPacket*>(data);
            cmdPacket->command[sizeof(cmdPacket->command) - 1] = '\0';
            if (OnServerCommandReceived) {
                OnServerCommandReceived(cmdPacket->command);
            }
        } else if (header->type == PacketType::TickSync) {
            SyncClock(header->tick);
        } else if (m_InGame && m_Scene && m_Replication.OnPacket(data, size, *this)) {
            if (header->type == PacketType::NetSnapshot) SyncClock(header->tick);
        }
    }
}

// ============================================================
// Entities
// ============================================================
void NetworkControl::EnsureReplicationScene() {
    auto* scene = dynamic_cast<DataDrivenScene*>(m_Scene);
    if (!m_ReplicationStale || !scene) return;
    m_ReplicationStale = false;
    m_ReplicationServer.BeginScene(scene->registry, scene->GetSceneEntities(), scene->GetSceneBaseline());
}

// The scene was just loaded and the local player created. From here on the server moves everything else.
void NetworkControl::BeginClientScene() {
    m_Remote.clear();
    m_VelocityCorrections.clear();
    m_Replication.Reset(m_PendingEpoch);

    if (auto* scene = dynamic_cast<DataDrivenScene*>(m_Scene)) {
        const auto& entities = scene->GetSceneEntities();
        if (entities.size() != m_PendingSceneCount) {
            ENGINE_ERROR("The scene has %zu entities, the server has %u", entities.size(), m_PendingSceneCount);
            return;
        }
        for (uint32_t i = 0; i < entities.size(); ++i) {
            RemoteEntity remote;
            remote.entity = entities[i];
            if (scene->registry.HasComponent<RigidBodyComponent>(entities[i])) {
                auto& rb = scene->registry.GetComponent<RigidBodyComponent>(entities[i]);
                // Only bodies with a collider are moved by the physics system
                if (rb.GetType() != BodyType::Static && scene->registry.HasComponent<ColliderComponent>(entities[i])) {
                    remote.body = true;
                    rb.SetType(BodyType::Kinematic);
                    rb.SetUseGravity(false);
                    rb.SetDrag(0.0f);
                }
            }
            if (scene->registry.HasComponent<AnimatorComponent>(entities[i]))
                remote.animCommand = scene->registry.GetComponent<AnimatorComponent>(entities[i]).GetCommandCount();
            m_Remote[i + 1] = remote;
        }
    }
    RemoteEntity self;
    self.entity = m_MyLocalPlayerID;
    self.player = self.body = true;
    m_Remote[m_MyServerPlayerID] = self;
}

void NetworkControl::OnNetSpawn(const NetSpawn& spawn, uint32_t tick) {
    RemoteEntity remote;
    auto& registry = m_Scene->registry;
    const NetLook& look = spawn.state.look;

    if (spawn.kind == NetSpawn::kPlayer) {
        remote.entity = OnSpawnPlayer(nullptr, false);
        remote.player = true;
        remote.body = true;
        if (remote.entity == 0) return;
    } else {
        remote.entity = m_Scene->CreateEntity(static_cast<EntityCategory>(spawn.category), spawn.name);
        registry.AddComponent<TransformComponent>(remote.entity);
        if (look.flags & NetLook::kSprite) registry.AddComponent<SpriteComponent2D>(remote.entity);
        if (look.flags & NetLook::kAnimator) registry.AddComponent<AnimatorComponent>(remote.entity);
        if (look.flags & NetLook::kCollider) {
            ColliderComponent collider(spawn.colliderOffset, spawn.colliderSize, (look.flags & NetLook::kTrigger) != 0);
            collider.SetAutoBounds(spawn.autoBounds);
            collider.SetLayerMask(spawn.layerMask);
            registry.AddComponent<ColliderComponent>(remote.entity, collider);
            RigidBodyComponent rb;
            remote.body = (look.flags & NetLook::kBody) != 0;
            rb.SetType(remote.body ? BodyType::Kinematic : BodyType::Static);
            registry.AddComponent<RigidBodyComponent>(remote.entity, rb);
        }
    }
    m_Remote[spawn.netId] = remote;
    if (registry.HasComponent<TransformComponent>(remote.entity))
        registry.GetComponent<TransformComponent>(remote.entity).position = spawn.state.motion.Position();
    OnNetLook(spawn.netId, look);
    OnNetMotion(spawn.netId, spawn.state.motion, tick);
}

void NetworkControl::OnNetDestroy(uint32_t netId) {
    const auto it = m_Remote.find(netId);
    if (it == m_Remote.end() || it->second.entity == m_MyLocalPlayerID) return;
    m_VelocityCorrections.erase(it->second.entity);
    if (it->second.player) OnDestroyPlayer(it->second.entity);
    else m_Scene->registry.Destroy(it->second.entity); // the server lists children separately
    m_Remote.erase(it);
}

void NetworkControl::OnNetLook(uint32_t netId, const NetLook& look) {
    RemoteEntity& remote = m_Remote[netId];
    auto& registry = m_Scene->registry;
    const EntityID e = remote.entity;
    if (registry.HasComponent<TransformComponent>(e))
        registry.GetComponent<TransformComponent>(e).size = {look.width / 256.0f, look.height / 256.0f};
    if (registry.HasComponent<ColliderComponent>(e))
        registry.GetComponent<ColliderComponent>(e).SetTrigger((look.flags & NetLook::kTrigger) != 0);

    if ((look.flags & NetLook::kSprite) && registry.HasComponent<SpriteComponent2D>(e)) {
        auto& sprite = registry.GetComponent<SpriteComponent2D>(e);
        sprite.frameIndex = look.frame;
        sprite.layer = static_cast<Layer>(look.layer);
        if (m_AssetManager && !look.sprite.empty() && look.sprite != sprite.objectId) {
            try {
                sprite.spriteSheet = m_AssetManager->GetSpriteSheet(look.sprite);
                sprite.objectId = look.sprite;
            } catch (const std::out_of_range&) {
                ENGINE_WARN("Unknown sprite '%s'", look.sprite.c_str());
            }
        }
    }
    if ((look.flags & NetLook::kAnimator) && registry.HasComponent<AnimatorComponent>(e)) {
        auto& animator = registry.GetComponent<AnimatorComponent>(e);
        bool rebound = false;
        if (m_AssetManager && !look.animator.empty() && look.animator != animator.objectId) {
            try {
                animator.BindAnimationSet(m_AssetManager->GetAnimationSet(look.animator), m_AssetManager->GetSpriteSheet(look.animator));
                animator.objectId = look.animator;
                rebound = true;
            } catch (const std::out_of_range&) {
                ENGINE_WARN("Unknown animations '%s'", look.animator.c_str());
            }
        }
        animator.SetSpeed(look.animSpeed / 64.0f);
        if (rebound || look.animCommand != remote.animCommand) {
            remote.animCommand = look.animCommand;
            if (look.flags & NetLook::kAnimStopped) animator.Stop();
            else animator.Play(look.clip, (look.flags & NetLook::kAnimLoop) != 0);
        }
    }
}

void NetworkControl::OnNetMotion(uint32_t netId, const NetMotion& motion, uint32_t tick) {
    RemoteEntity& remote = m_Remote[netId];
    auto& registry = m_Scene->registry;
    if (remote.entity == m_MyLocalPlayerID || !registry.HasComponent<TransformComponent>(remote.entity)) return;
    auto& trans = registry.GetComponent<TransformComponent>(remote.entity);
    glm::vec2& position = trans.parentTransform ? trans.localPosition : trans.position;
    const glm::vec2 velocity = motion.Velocity();

    trans.rotation = motion.Rotation();
    remote.velocity = velocity;
    if (remote.body && registry.HasComponent<RigidBodyComponent>(remote.entity))
        registry.GetComponent<RigidBodyComponent>(remote.entity).SetVelocity(velocity);

    // Project server position to current client tick
    // Bounded, so a bad tick can't fling entities away
    const int ticksAhead = std::clamp(static_cast<int32_t>(GetClientTick() - tick), -120, 120);
    const glm::vec2 projected = motion.Position() + velocity * (static_cast<float>(ticksAhead) / 60.0f);
    const glm::vec2 error = projected - position;
    const float dist = glm::length(error);

    if (dist > 2.0f) {
        // Completely desynced or first spawn: snap
        position = projected;
        m_VelocityCorrections.erase(remote.entity);
    } else if (remote.player && glm::length(velocity) < 0.1f && dist < 0.5f) {
        // If a player stopped and the error is just a small latency overshoot,
        // DO NOT moonwalk them backward. Hide the error until they start moving again!
        m_VelocityCorrections.erase(remote.entity);
    } else {
        // A velocity that smoothly closes the gap over ~0.2s
        m_VelocityCorrections[remote.entity] = error * 5.0f;
    }
}

void NetworkControl::OnNetOwnPosition(const glm::vec2& serverPos, uint32_t serverUpdateTick) {
    if (m_MyLocalPlayerID == 0 || !m_Scene->registry.HasComponent<TransformComponent>(m_MyLocalPlayerID)) return;
    auto& trans = m_Scene->registry.GetComponent<TransformComponent>(m_MyLocalPlayerID);
    const auto stop = [&] {
        if (m_Scene->registry.HasComponent<RigidBodyComponent>(m_MyLocalPlayerID))
            m_Scene->registry.GetComponent<RigidBodyComponent>(m_MyLocalPlayerID).SetVelocity(glm::vec2(0.0f));
    };

    const uint32_t reconTick = serverUpdateTick + 3;
    const auto historyIt = m_ClientStateHistory.find(reconTick);
    if (historyIt != m_ClientStateHistory.end()) {
        const glm::vec2 error = serverPos - historyIt->second.position;
        const float errorLen = glm::length(error);
        if (errorLen > 1.5f) {
            // Shift by the error instead of moving to serverPos: that is where the player was some
            // ticks ago, so a fast player would be wrong again right away
            trans.position += error;
            for (auto& [tick, state] : m_ClientStateHistory)
                if (tick >= reconTick) state.position += error;
            ENGINE_LOG("[Client] Large desync detected (%.2f units) at tick %u. Snapping.", errorLen, reconTick);
        } else if (errorLen > 0.001f) {
            const glm::vec2 correction = error * 0.5f;
            trans.position += correction;
            for (auto& [tick, state] : m_ClientStateHistory)
                if (tick > reconTick) state.position += correction;
        }
    } else {
        const float dist = glm::distance(trans.position, serverPos);
        if (dist > 1.5f) {
            trans.position = serverPos;
            stop();
            ENGINE_LOG("[Client] No history for tick %u and large desync (%.2f units). Snapping.", reconTick, dist);
        } else {
            trans.position = glm::mix(trans.position, serverPos, 0.4f);
        }
    }
}

void NetworkControl::RememberPlayerPositions() {
    m_RememberedPositions.clear();
    if (!m_Scene) return;
    for (auto const& [peer, entID] : m_PeerToEntity)
        if (m_Scene->registry.HasComponent<TransformComponent>(entID))
            m_RememberedPositions[peer] = m_Scene->registry.GetComponent<TransformComponent>(entID).position;
}

void NetworkControl::OnClientDisconnected(ENetPeer* peer) {
    m_ReplicationServer.RemovePeer(peer);
    m_RememberedPositions.erase(peer);
    m_BufferedPeerInputs.erase(peer);
    m_LastExecutedInputs.erase(peer);
    if (m_PeerToEntity.find(peer) != m_PeerToEntity.end()) {
        EntityID entID = m_PeerToEntity[peer];
        OnDestroyPlayer(entID);
        m_PeerToEntity.erase(peer);
    }
}

std::vector<EntityID> NetworkControl::GetActivePlayerEntities() const {
    std::vector<EntityID> players;
    for (auto const& [peer, entID] : m_PeerToEntity) {
        players.push_back(entID);
    }
    return players;
}

void NetworkControl::OnSceneChanged() {
    m_MyLocalPlayerID = 0;
    // m_MyServerPlayerID stays: the scene is often reloaded between the welcome and the spawn
    m_PeerToEntity.clear();
    m_Remote.clear();
    m_BufferedPeerInputs.clear();
    m_LastExecutedInputs.clear();
    m_ClientStateHistory.clear();
    m_ClientInputHistory.clear();
    m_VelocityCorrections.clear();
    m_InGame = false;
    m_ReplicationStale = true;
    ENGINE_LOG("NetworkControl: Cleared connection entities and ticks for scene transition.");
}

void NetworkControl::SpawnLocalPlayer() {
    m_MyLocalPlayerID = OnSpawnPlayer(nullptr, true);
    if (m_NetService && m_NetService->GetMode() == NetworkMode::Client) {
        m_InGame = true;
        BeginClientScene();
    }
}
