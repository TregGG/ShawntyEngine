#pragma once
#include "../services/networkcontrol.h"
#include <vector>
#include <memory>

class AssetManager;
class GameObject;
class TestPlayer;

class TestNetworkControl : public NetworkControl {
public:
    TestNetworkControl(AssetManager* assets);
    ~TestNetworkControl() override;
    void OnSceneChanged() override;
    void AddMockPlayer(ENetPeer* peer, EntityID entityId) {
        m_PeerToEntity[peer] = entityId;
    }

protected:
    EntityID OnSpawnPlayer(ENetPeer* peer, bool isLocal) override;
    void OnDestroyPlayer(EntityID entity) override;
    uint16_t OnGenerateInputMask(Input* input) override;
    void OnApplyInput(EntityID entity, uint16_t inputMask) override;

private:
    AssetManager* m_Assets = nullptr;
    std::vector<std::unique_ptr<GameObject>> m_ManagedObjects;
    bool m_IsFrozen = false;
    std::unordered_map<EntityID, int> m_CoyoteTicks;
};
