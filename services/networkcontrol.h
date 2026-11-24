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
#include "clientclock.h"
#include "replication.h"
#include "../services/base/entityregistry/entityregistry.h" // For EntityID

class NetworkService;
class ContentServer;
class ContentClient;
class Scene;
class Input;

class AssetManager;

class NetworkControl : public Service, private ReplicationClient::Sink {
public:
    NetworkControl();
    virtual ~NetworkControl() override;

    void Init() override;
    void Update(float dt) override;
    void Shutdown() override;
    
    // Core game loop ticks this
    virtual void Tick(float tickInterval);

    void SetNetworkService(NetworkService* netService);
    // With content sync, clients must hold the current project version before they play.
    void SetContentServer(ContentServer* content) { m_ContentServer = content; }
    void SetContentClient(ContentClient* content) { m_ContentClient = content; }

    // Players rejoining after a hot reload continue where they were.
    void RememberPlayerPositions();
    void ForgetPlayerPositions() { m_RememberedPositions.clear(); }

    void SetAssets(AssetManager* assets) { m_AssetManager = assets; }

    void BindScene(Scene* scene, Input* input) { m_Scene = scene; m_Input = input; }

    // Client/Host calls this when user types in console/UI
    virtual void ProcessCommandString(const std::string& command);

    // Remote interpolation data
    std::unordered_map<EntityID, glm::vec2> m_VelocityCorrections;
    virtual void OnPacketReceived(ENetPeer* peer, void* data, size_t size);
    
    // NetworkService calls this when a client disconnects
    virtual void OnClientDisconnected(ENetPeer* peer);
    
    uint32_t GetLocalPlayerID() const { return m_MyLocalPlayerID; }
    uint32_t GetServerTick() const { return m_ServerTick; }
    uint32_t GetClientTick() const { return m_Clock.GetTick(); }
    static void SetPrintStats(bool enabled) { s_PrintStats = enabled; }

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
    ReplicationServer m_ReplicationServer;
    bool m_ReplicationStale = true;
    void EnsureReplicationScene();
    void SendToPlayers(const void* data, size_t size, enet_uint32 flags);
    std::vector<ENetPeer*> m_Receivers;
    inline static bool s_PrintStats = false;
    uint32_t m_InputsUsed = 0;
    uint32_t m_InputsMissing = 0;
    ContentServer* m_ContentServer = nullptr;
    std::map<ENetPeer*, glm::vec2> m_RememberedPositions;
    
    // Client state
    ContentClient* m_ContentClient = nullptr;
    bool m_InGame = false; // welcomed into the current scene
    AssetManager* m_AssetManager = nullptr;
    ReplicationClient m_Replication;
    uint16_t m_PendingEpoch = 0;
    uint32_t m_PendingSceneCount = 0;
    struct RemoteEntity
    {
        EntityID entity = 0;
        bool player = false;
        bool body = false;             // physics moves it
        glm::vec2 velocity{0.0f};      // otherwise this does
        uint8_t animCommand = 0;
    };
    std::unordered_map<uint32_t, RemoteEntity> m_Remote; // by network ID
    void BeginClientScene();
    bool OnNetKnows(uint32_t netId) override { return m_Remote.count(netId) > 0; }
    void OnNetSpawn(const NetSpawn& spawn, uint32_t tick) override;
    void OnNetDestroy(uint32_t netId) override;
    void OnNetMotion(uint32_t netId, const NetMotion& motion, uint32_t tick) override;
    void OnNetLook(uint32_t netId, const NetLook& look) override;
    void OnNetOwnPosition(const glm::vec2& position, uint32_t tick) override;
    uint32_t m_MyServerPlayerID = 0;
    EntityID m_MyLocalPlayerID = 0;
    ClientClock m_Clock;
    uint16_t m_RecentInputs[ClientInputPacket::kInputs] = {};
    void RunClientTick(uint16_t inputMask);
    void SyncClock(uint32_t serverTick);
    
    struct ClientState {
        glm::vec2 position;
        glm::vec2 velocity;
    };
    std::map<uint32_t, ClientState> m_ClientStateHistory;
    std::map<uint32_t, uint16_t> m_ClientInputHistory;
    
    // Helper to send packets
    void SendClientInput(uint16_t inputMask, float mouseX, float mouseY);
};
