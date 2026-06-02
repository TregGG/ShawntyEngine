#include "networkcontrol.h"
#include "networkservice.h"
#include "../levels/scene.h"
#include "../levels/datadrivenscene.h"
#include "../core/input.h"
#include "../core/logger.h"
#include "../objects/components/components.h"
#include "../objects/components/rigidbodycomponent.h"
#include <algorithm>
#include <cstring>

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
        };
    }
}

void NetworkControl::Update(float dt) {
    if (m_NetService && m_NetService->GetMode() == NetworkMode::Client && m_Scene) {
        for (auto const& [sID, localID] : m_ServerToLocalEntity) {
            if (localID != m_MyLocalPlayerID) {
                if (m_Scene->registry.HasComponent<TransformComponent>(localID)) {
                    
                    auto& trans = m_Scene->registry.GetComponent<TransformComponent>(localID);
                    
                    auto it = m_VelocityCorrections.find(localID);
                    if (it != m_VelocityCorrections.end()) {
                        // Apply the correction velocity to the physical position
                        trans.position += it->second * dt;
                        
                        // Decay the correction so it smoothly stops
                        it->second *= std::exp(-5.0f * dt);
                        if (glm::length(it->second) < 0.01f) {
                            it->second = glm::vec2(0.0f);
                        }
                    }
                }
            }
        }
    }
}

void NetworkControl::Tick(float tickInterval) {
    if (!m_NetService || !m_Scene) return;

    if (m_NetService->GetMode() == NetworkMode::Server) {
        m_ServerTick++;
        
        // 1. Process client inputs (SimulateServerTick logic)
        for (auto const& [peer, entID] : m_PeerToEntity) {
            uint16_t lastInput = m_LastExecutedInputs[peer];
            auto peerInputsIt = m_BufferedPeerInputs.find(peer);
            
            if (peerInputsIt != m_BufferedPeerInputs.end()) {
                auto inputIt = peerInputsIt->second.find(m_ServerTick);
                if (inputIt != peerInputsIt->second.end()) {
                    lastInput = inputIt->second;
                    m_LastExecutedInputs[peer] = lastInput;
                }
            }
            
            OnApplyInput(entID, lastInput);
            
            // Garbage collect old inputs
            if (peerInputsIt != m_BufferedPeerInputs.end()) {
                for (auto it = peerInputsIt->second.begin(); it != peerInputsIt->second.end(); ) {
                    if (it->first < m_ServerTick - 100) {
                        it = peerInputsIt->second.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        
        // 2. Broadcast TickSync
        if (m_ServerTick % 60 == 0) {
            PacketHeader tickHeader;
            tickHeader.type = PacketType::TickSync;
            tickHeader.tick = m_ServerTick;
            m_NetService->BroadcastPacket(0, &tickHeader, sizeof(PacketHeader), ENET_PACKET_FLAG_RELIABLE);
        }

        // 3. Broadcast StateSync (every 15 ticks) or ServerUpdate
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
                if (m_Scene->registry.HasComponent<TransformComponent>(entID)) {
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
                if (m_Scene->registry.HasComponent<RigidBodyComponent>(entID)) {
                    auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(entID);
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
        if (m_Input) {
            inputMask = OnGenerateInputMask(m_Input);
        }
        
        SendClientInput(inputMask, 0.0f, 0.0f);
        m_ClientInputHistory[m_ClientTick] = inputMask;
        
        // Clean history
        for (auto it = m_ClientInputHistory.begin(); it != m_ClientInputHistory.end();) {
            if (it->first < m_ClientTick - 100) it = m_ClientInputHistory.erase(it);
            else ++it;
        }
        for (auto it = m_ClientStateHistory.begin(); it != m_ClientStateHistory.end();) {
            if (it->first < m_ClientTick - 100) it = m_ClientStateHistory.erase(it);
            else ++it;
        }
        
        // Apply input locally for prediction
        if (m_MyLocalPlayerID != 0) {
            OnApplyInput(m_MyLocalPlayerID, inputMask);
            
            if (m_Scene->registry.HasComponent<TransformComponent>(m_MyLocalPlayerID) && m_Scene->registry.HasComponent<RigidBodyComponent>(m_MyLocalPlayerID)) {
                auto& trans = m_Scene->registry.GetComponent<TransformComponent>(m_MyLocalPlayerID);
                auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(m_MyLocalPlayerID);
                m_ClientStateHistory[m_ClientTick] = { trans.position, rb.GetVelocity() };
            }
        }
    }
}

void NetworkControl::SendClientInput(uint16_t inputMask, float mouseX, float mouseY) {
    if (!m_NetService) return;
    ClientInputPacket packet;
    packet.header.type = PacketType::ClientInput;
    packet.header.tick = m_ClientTick;
    packet.inputMask = inputMask;
    packet.mouseDeltaX = mouseX;
    packet.mouseDeltaY = mouseY;
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

    if (m_NetService->GetMode() == NetworkMode::Server) {
        if (header->type == PacketType::ClientInput && size >= sizeof(ClientInputPacket)) {
            if (m_PeerToEntity.find(peer) == m_PeerToEntity.end()) {
                EntityID pID = OnSpawnPlayer(peer, false);
                m_PeerToEntity[peer] = pID;
                
                ConnectPacket welcomePacket;
                welcomePacket.header.type = PacketType::Connect;
                welcomePacket.header.tick = m_ServerTick;
                welcomePacket.clientEntityID = pID;
                
                std::string scenePath = "";
                if (m_Scene && dynamic_cast<DataDrivenScene*>(m_Scene)) {
                    scenePath = dynamic_cast<DataDrivenScene*>(m_Scene)->GetSceneFilePath();
                }
                strncpy(welcomePacket.sceneName, scenePath.c_str(), sizeof(welcomePacket.sceneName));
                welcomePacket.sceneName[sizeof(welcomePacket.sceneName) - 1] = '\0';
                
                m_NetService->SendPacket(peer, 0, &welcomePacket, sizeof(welcomePacket), ENET_PACKET_FLAG_RELIABLE);
            }
            
            ClientInputPacket* inputPacket = reinterpret_cast<ClientInputPacket*>(data);
            uint32_t targetTick = inputPacket->header.tick;
            m_BufferedPeerInputs[peer][targetTick] = inputPacket->inputMask;
        }
    } else if (m_NetService->GetMode() == NetworkMode::Client) {
        if (header->type == PacketType::Connect && size >= sizeof(ConnectPacket)) {
            ConnectPacket* connectPacket = reinterpret_cast<ConnectPacket*>(data);
            
            // Clean up old state completely, because this is a fresh connection or scene transition
            m_ClientTick = 0;
            m_ServerTick = 0;
            m_MyLocalPlayerID = 0;
            m_PeerToEntity.clear();
            m_ServerToLocalEntity.clear();
            m_BufferedPeerInputs.clear();
            m_LastExecutedInputs.clear();
            m_ClientStateHistory.clear();
            m_ClientInputHistory.clear();

            m_MyServerPlayerID = connectPacket->clientEntityID;
            
            uint32_t serverTick = connectPacket->header.tick;
            uint32_t rttTicks = 0;
            if (m_NetService->GetServerPeer()) {
                rttTicks = (m_NetService->GetServerPeer()->roundTripTime / 2) / 16.67f;
            }
            m_LatencyOffsetTicks = rttTicks + 5;
            m_ClientTick = serverTick + m_LatencyOffsetTicks;
            
            std::string sceneName(connectPacket->sceneName);
            if (OnClientConnectedCallback) {
                OnClientConnectedCallback(sceneName);
            } else {
                SpawnLocalPlayer();
            }
        } else if (header->type == PacketType::ServerUpdate) {
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
                    EntityID lID = OnSpawnPlayer(nullptr, false);
                    m_ServerToLocalEntity[sID] = lID;
                }
                
                EntityID localID = m_ServerToLocalEntity[sID];
                if (localID != m_MyLocalPlayerID) {
                    if (m_Scene->registry.HasComponent<RigidBodyComponent>(localID)) {
                        auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(localID);
                        rb.SetVelocity(glm::vec2(velocities[i].vx, velocities[i].vy));
                    }
                }
            }
            
            std::vector<uint32_t> toRemove;
            for (auto const& [sID, lID] : m_ServerToLocalEntity) {
                if (std::find(activeServerIDs.begin(), activeServerIDs.end(), sID) == activeServerIDs.end()) {
                    toRemove.push_back(sID);
                }
            }
            
            for (uint32_t sID : toRemove) {
                OnDestroyPlayer(m_ServerToLocalEntity[sID]);
                m_ServerToLocalEntity.erase(sID);
            }
        } else if (header->type == PacketType::ServerCommand && size >= sizeof(ServerCommandPacket)) {
            ServerCommandPacket* cmdPacket = reinterpret_cast<ServerCommandPacket*>(data);
            if (OnServerCommandReceived) {
                OnServerCommandReceived(cmdPacket->command);
            }
        } else if (header->type == PacketType::TickSync) {
            uint32_t targetClientTick = header->tick + m_LatencyOffsetTicks;
            int diff = (int)m_ClientTick - (int)targetClientTick;
            if (std::abs(diff) > 3) {
                m_ClientTick = targetClientTick;
                m_ClientStateHistory.clear();
                m_ClientInputHistory.clear();
                ENGINE_LOG("[Client] Clock drifted by %d ticks. Resynchronized client tick to %u.", diff, m_ClientTick);
            }
        } else if (header->type == PacketType::StateSync) {
            size_t countOffset = sizeof(PacketHeader);
            if (size < countOffset + sizeof(uint32_t)) return;
            uint32_t count = *reinterpret_cast<uint32_t*>(static_cast<char*>(data) + countOffset);
            
            size_t dataOffset = countOffset + sizeof(uint32_t);
            if (size < dataOffset + count * sizeof(EntityPositionData)) return;
            EntityPositionData* positions = reinterpret_cast<EntityPositionData*>(static_cast<char*>(data) + dataOffset);
            
            uint32_t serverUpdateTick = header->tick;
            uint32_t targetClientTick = serverUpdateTick + m_LatencyOffsetTicks;
            int diff = (int)m_ClientTick - (int)targetClientTick;
            if (std::abs(diff) > 3) {
                m_ClientTick = targetClientTick;
                m_ClientStateHistory.clear();
                m_ClientInputHistory.clear();
                ENGINE_LOG("[Client StateSync] Clock drifted by %d ticks. Resynchronized client tick to %u.", diff, m_ClientTick);
            }
            
            std::vector<uint32_t> activeServerIDs;
            activeServerIDs.reserve(count);
            
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t sID = positions[i].entityID;
                activeServerIDs.push_back(sID);
                
                if (m_ServerToLocalEntity.find(sID) == m_ServerToLocalEntity.end()) {
                    EntityID lID = OnSpawnPlayer(nullptr, false);
                    m_ServerToLocalEntity[sID] = lID;
                }
                
                EntityID localID = m_ServerToLocalEntity[sID];
                if (m_Scene->registry.HasComponent<TransformComponent>(localID)) {
                    auto& trans = m_Scene->registry.GetComponent<TransformComponent>(localID);
                    glm::vec2 serverPos(positions[i].x, positions[i].y);
                    
                    if (localID == m_MyLocalPlayerID) {
                        uint32_t reconTick = serverUpdateTick + 3;
                        auto historyIt = m_ClientStateHistory.find(reconTick);
                        if (historyIt != m_ClientStateHistory.end()) {
                            glm::vec2 predictedPos = historyIt->second.position;
                            glm::vec2 error = serverPos - predictedPos;
                            float errorLen = glm::length(error);
                            
                            if (errorLen > 1.5f) {
                                trans.position = serverPos;
                                if (m_Scene->registry.HasComponent<RigidBodyComponent>(localID)) {
                                    m_Scene->registry.GetComponent<RigidBodyComponent>(localID).SetVelocity(glm::vec2(0.0f));
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
                                if (m_Scene->registry.HasComponent<RigidBodyComponent>(localID)) {
                                    m_Scene->registry.GetComponent<RigidBodyComponent>(localID).SetVelocity(glm::vec2(0.0f));
                                }
                                ENGINE_LOG("[Client] No history for tick %u and large desync (%.2f units). Snapping.", reconTick, dist);
                            } else {
                                trans.position = glm::mix(trans.position, serverPos, 0.4f);
                            }
                        }
                    } else {
                        // Project server position to current client tick
                        int ticksAhead = (int)m_ClientTick - (int)(serverUpdateTick + 3);
                        glm::vec2 projectedPos = serverPos;
                        glm::vec2 clientVel(0.0f);
                        if (m_Scene->registry.HasComponent<RigidBodyComponent>(localID)) {
                            auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(localID);
                            clientVel = rb.GetVelocity();
                            if (ticksAhead != 0) {
                                projectedPos += clientVel * ((float)ticksAhead / 60.0f);
                            }
                        }

                        // Compare projected position with current position
                        glm::vec2 error = projectedPos - trans.position;
                        float dist = glm::length(error);
                        
                        // If completely desynced or first spawn, snap instantly
                        if (dist > 2.0f) {
                            trans.position = projectedPos;
                            m_VelocityCorrections[localID] = glm::vec2(0.0f);
                        } else {
                            // If they stopped moving and the error is just a small latency overshoot,
                            // DO NOT moonwalk them backward. Hide the error until they start moving again!
                            if (glm::length(clientVel) < 0.1f && dist < 0.5f) {
                                m_VelocityCorrections[localID] = glm::vec2(0.0f);
                            } else {
                                // Otherwise calculate a velocity to smoothly close the gap over ~0.2s
                                m_VelocityCorrections[localID] = error * 5.0f;
                            }
                        }
                    }
                }
            }
        }
    }
}

void NetworkControl::OnClientDisconnected(ENetPeer* peer) {
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
    m_MyServerPlayerID = 0;
    m_PeerToEntity.clear();
    m_ServerToLocalEntity.clear();
    m_BufferedPeerInputs.clear();
    m_LastExecutedInputs.clear();
    m_ClientStateHistory.clear();
    m_ClientInputHistory.clear();
    m_VelocityCorrections.clear();
    ENGINE_LOG("NetworkControl: Cleared connection entities and ticks for scene transition.");
}

void NetworkControl::SpawnLocalPlayer() {
    m_MyLocalPlayerID = OnSpawnPlayer(nullptr, true);
    m_ServerToLocalEntity[m_MyServerPlayerID] = m_MyLocalPlayerID;
}
