#pragma once

#include "service.h"
#include <string>
#include <map>
#include <cstdint>
#include <vector>
#include <functional>
#include <glm/vec2.hpp>
#include <enet/enet.h>
#include "../core/network_data.h"
#include "../services/base/entityregistry/entityregistry.h" // For EntityID

class NetworkService;
class Scene;
class Input;

class NetworkControl : public Service {
public:
    NetworkControl();
    virtual ~NetworkControl() override;

    void Init() override;
    void Update(float dt) override;
    void Shutdown() override;
    
    // Core game loop ticks this
    virtual void Tick(float tickInterval);

    void SetNetworkService(NetworkService* netService);
    void BindScene(Scene* scene, Input* input) { m_Scene = scene; m_Input = input; }

    // Client/Host calls this when user types in console/UI
    virtual void ProcessCommandString(const std::string& command);

    // NetworkService calls this when an ENet packet is received
    virtual void OnPacketReceived(ENetPeer* peer, void* data, size_t size);
    
    // NetworkService calls this when a client disconnects
    virtual void OnClientDisconnected(ENetPeer* peer);
    
    uint32_t GetLocalPlayerID() const { return m_MyLocalPlayerID; }
    uint32_t GetServerTick() const { return m_ServerTick; }
    uint32_t GetClientTick() const { return m_ClientTick; }

    std::function<void(const std::string&)> OnServerCommandReceived;
    std::function<void(const std::string&)> OnClientConnectedCallback;
    std::vector<EntityID> GetActivePlayerEntities() const;
    virtual void OnSceneChanged();
    void SpawnLocalPlayer();

protected:
    // Game-specific virtual callbacks
    virtual EntityID OnSpawnPlayer(ENetPeer* /*peer*/, bool /*isLocal*/) { return 0; }
    virtual void OnDestroyPlayer(EntityID /*entity*/) {}
    virtual uint16_t OnGenerateInputMask(Input* /*input*/) { return 0; }
    virtual void OnApplyInput(EntityID /*entity*/, uint16_t /*inputMask*/) {}
    
    // Default prediction logic
    virtual glm::vec2 ProjectPlayerState(EntityID entID, ENetPeer* peer, uint32_t startTick, int numTicks);

    NetworkService* m_NetService = nullptr;
    Scene* m_Scene = nullptr;
    Input* m_Input = nullptr;
    
    // Server state
    std::map<ENetPeer*, EntityID> m_PeerToEntity;
    std::map<ENetPeer*, std::map<uint32_t, uint16_t>> m_BufferedPeerInputs;
    std::map<ENetPeer*, uint16_t> m_LastExecutedInputs;
    uint32_t m_ServerTick = 0;
    
    // Client state
    std::map<uint32_t, EntityID> m_ServerToLocalEntity;
    uint32_t m_MyServerPlayerID = 0;
    EntityID m_MyLocalPlayerID = 0;
    uint32_t m_ClientTick = 0;
    uint32_t m_LatencyOffsetTicks = 5;
    
    struct ClientState {
        glm::vec2 position;
        glm::vec2 velocity;
    };
    std::map<uint32_t, ClientState> m_ClientStateHistory;
    std::map<uint32_t, uint16_t> m_ClientInputHistory;
    
    // Helper to send packets
    void SendClientInput(uint16_t inputMask, float mouseX, float mouseY);
};
