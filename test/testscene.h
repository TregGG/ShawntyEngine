#pragma once
#include "../levels/scene.h"
#include <vector>
#include <memory>
#include <glm/vec2.hpp>
#include "../services/base/entityregistry/entityregistry.h"
#include "../services/base/physics/physicssystem.h"
#include "../services/base/raycast.h"
#include "../core/network_data.h"
#include <map>
#include <enet/enet.h>

class GameObject;
class AssetManager;
class Input;

#include "../objects/ui/uipanel.h"
#include "../objects/ui/uitext.h"
#include "../objects/ui/uibutton.h"
#include "../objects/ui/uiinputfield.h"
#include "../render/fontengine.h"
#include "../services/base/eventservice.h"

class NetworkService;

class TestScene : public Scene
{
public:
    TestScene(AssetManager* assets, EventService* eventService, FontEngine* fontEngine, NetworkService* netService)
        : Scene(assets), m_EventService(eventService), m_FontEngine(fontEngine), m_NetService(netService) {}

    void OnEnter() override;
    void OnExit() override;
    void Update(float deltatime) override;
    void BuildDebugRenderables(std::vector<DebugRect>& outDebugRects) const override;
    void BuildDebugLines(std::vector<DebugLine>& outDebugLines) const override;
    
    void OnNetworkPacket(ENetPeer* peer, void* data, size_t size);
    void OnClientDisconnected(ENetPeer* peer);

private:

    // std::vector<std::unique_ptr<GameObject>> m_GameObjects;
    float m_MoveSpeed = 5.0f;
    PhysicsSystem m_Physics;
    EventService* m_EventService = nullptr;
    FontEngine* m_FontEngine = nullptr;
    NetworkService* m_NetService = nullptr;
    UIText* m_StatusText = nullptr;
    bool m_UIHidden = false;
    
    std::map<ENetPeer*, EntityID> m_PeerToEntity; // For server: Peer -> Player Entity
    std::map<uint32_t, EntityID> m_ServerToLocalEntity; // For client: ServerEntityID -> Local Dummy Entity
    uint32_t m_MyServerPlayerID = 0;
    EntityID m_MyLocalPlayerID = 0;
    
    struct ClientState {
        glm::vec2 position;
        glm::vec2 velocity;
    };
    
    float m_TimeAccumulator = 0.0f;
    uint32_t m_ServerTick = 0;
    uint32_t m_ClientTick = 0;
    uint32_t m_LatencyOffsetTicks = 5;
    std::map<ENetPeer*, std::map<uint32_t, uint16_t>> m_BufferedPeerInputs; // peer -> (tick -> inputMask)
    std::map<ENetPeer*, uint16_t> m_LastExecutedInputs; // peer -> last inputMask
    
    std::map<uint32_t, ClientState> m_ClientStateHistory;
    std::map<uint32_t, uint16_t> m_ClientInputHistory;
    bool m_SpaceWasPressed = false;
    
    void ApplyInput(EntityID entID, uint16_t inputMask);
    glm::vec2 ProjectPlayerState(EntityID entID, ENetPeer* peer, uint32_t startTick, int numTicks);
    void SimulateServerTick();
    
    std::vector<DebugLine> m_TestLines;
    std::vector<DebugRect> m_TestRects;
};
