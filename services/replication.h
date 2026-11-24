#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <enet/enet.h>
#include <glm/vec2.hpp>
#include "../core/entityid.h"
#include "../core/network_data.h"

class EntityRegistryService;
class NetworkService;

// Sends the state of every entity from the server to clients.
//
// - Network IDs are never reused within a scene. Scene file entities are 1..N in file order,
//   the same on both sides because clients load the identical file. Others count up from N+1.
// - Spawns and removals are reliable. Everything else goes into small unreliable snapshots that
//   contain only what a client hasn't acknowledged yet.
// - Positions are sent when they differ from what the client extrapolates from the velocity.
// - The epoch changes with every scene load, so packets about an older scene are ignored.

// What moves. Fixed point: positions 1/256, velocities 1/64, rotation 1/10430 rad.
struct NetMotion
{
    int32_t x = 0, y = 0, vx = 0, vy = 0;
    int16_t rotation = 0;

    glm::vec2 Position() const;
    glm::vec2 Velocity() const;
    float Rotation() const;
    void SetPosition(const glm::vec2& p);
    void SetVelocity(const glm::vec2& v);
    void SetRotation(float radians);
    bool operator==(const NetMotion& o) const
    {
        return x == o.x && y == o.y && vx == o.vx && vy == o.vy && rotation == o.rotation;
    }
};

// What rarely changes.
struct NetLook
{
    enum Flags : uint8_t { kSprite = 1, kAnimator = 2, kAnimLoop = 4, kAnimStopped = 8, kCollider = 16, kTrigger = 32, kBody = 64 };
    uint8_t flags = 0;
    uint8_t layer = 0;
    uint8_t animCommand = 0; // changes with every Play or Stop
    int32_t width = 256, height = 256, frame = 0, animSpeed = 64;
    std::string sprite, animator, clip;

    bool operator==(const NetLook& o) const
    {
        return flags == o.flags && layer == o.layer && animCommand == o.animCommand && width == o.width
            && height == o.height && frame == o.frame && animSpeed == o.animSpeed
            && sprite == o.sprite && animator == o.animator && clip == o.clip;
    }
};

struct NetState
{
    NetMotion motion;
    NetLook look;
};

struct NetSpawn
{
    enum Kind : uint8_t { kPlayer = 0, kGeneric = 1 };
    uint32_t netId = 0;
    uint8_t kind = kGeneric;
    uint8_t category = 0;
    std::string name;
    NetState state;
    // Generic entities with a collider
    bool autoBounds = true;
    glm::vec2 colliderOffset{0.0f}, colliderSize{1.0f};
    uint32_t layerMask = 0xFFFFFFFF;
};

// localSpace: children use their local position, as in the scene file
NetState CaptureNetState(const EntityRegistryService& registry, EntityID entity, bool localSpace);

class ReplicationServer
{
public:
    void BeginScene(EntityRegistryService& registry, const std::vector<EntityID>& sceneEntities,
                    const std::vector<NetState>& baseline);
    uint16_t GetEpoch() const { return m_Epoch; }
    uint32_t GetSceneEntityCount() const { return m_SceneCount; }

    uint32_t RegisterPlayer(NetworkService& net, EntityID entity, uint32_t tick);
    // After the peer was told about the epoch
    void AddPeer(NetworkService& net, ENetPeer* peer, EntityID own, uint32_t tick);
    void RemovePeer(ENetPeer* peer) { m_Peers.erase(peer); }
    void OnAck(ENetPeer* peer, uint16_t epoch, uint16_t seq, uint64_t bits);
    // ownPosition: what the peer should compare its own prediction with
    void Tick(NetworkService& net, uint32_t tick, const std::map<ENetPeer*, glm::vec2>& ownPositions);

private:
    static constexpr uint32_t kSendInterval = 2;     // ticks, so 30 times a second
    static constexpr uint32_t kOwnInterval = 15;
    static constexpr size_t kMaxPacket = 1150;       // stays below the MTU: ENet would send larger ones reliably
    static constexpr int kMaxPacketsPerSend = 4;
    static constexpr size_t kSentHistory = 256;

    struct Entry
    {
        EntityID entity = 0;
        uint32_t netId = 0;
        bool used = false, scene = false, player = false;
        NetState published;
        uint32_t publishedTick = 0;
        uint32_t motionVersion = 0, lookVersion = 0;
        glm::vec2 lastPosition{0.0f};
        uint32_t lastTick = 0;
        glm::vec2 derivedVelocity{0.0f};
        int stillSamples = 0;
    };
    struct PeerEntity
    {
        uint32_t netId = 0;
        uint32_t ackedMotion = 0, ackedLook = 0;
        uint32_t sentMotion = 0, sentLook = 0;
        uint32_t sentTick = 0;
    };
    struct SentRecord { uint32_t slot, netId, motion, look; };
    struct SentPacket { uint16_t seq = 0; bool pending = false; std::vector<SentRecord> records; };
    struct Peer
    {
        EntityID own = 0;
        uint16_t nextSeq = 1;
        size_t cursor = 0;
        std::vector<PeerEntity> entities;
        std::vector<SentPacket> sent;
    };

    uint32_t AddEntry(EntityID entity, uint32_t netId, bool scene, bool player);
    bool IsPartOfPlayer(EntityID entity) const;
    void UpdateEntities(NetworkService& net, uint32_t tick);
    void Sample(Entry& entry, uint32_t tick);
    NetSpawn DescribeSpawn(const Entry& entry, uint32_t tick) const;
    void SendSnapshots(NetworkService& net, ENetPeer* peer, Peer& state, uint32_t tick, const glm::vec2* own, bool entities);

    EntityRegistryService* m_Registry = nullptr;
    uint16_t m_Epoch = 0;
    uint32_t m_SceneCount = 0;
    uint32_t m_NextNetId = 1;
    std::vector<Entry> m_Entries;
    std::vector<uint32_t> m_FreeSlots;
    std::unordered_map<EntityID, uint32_t> m_SlotOf;
    std::vector<uint32_t> m_RemovedSceneIds;
    std::map<ENetPeer*, Peer> m_Peers;
};

// Decodes packets from the server. Everything is bounds checked; the sink decides what to do with it.
class ReplicationClient
{
public:
    struct Sink
    {
        virtual ~Sink() = default;
        virtual bool OnNetKnows(uint32_t netId) = 0;
        virtual void OnNetSpawn(const NetSpawn& spawn, uint32_t tick) = 0;
        virtual void OnNetDestroy(uint32_t netId) = 0;
        virtual void OnNetMotion(uint32_t netId, const NetMotion& motion, uint32_t tick) = 0;
        virtual void OnNetLook(uint32_t netId, const NetLook& look) = 0;
        virtual void OnNetOwnPosition(const glm::vec2& position, uint32_t tick) = 0;
    };

    void Reset(uint16_t epoch);
    // True if it was a replication packet
    bool OnPacket(const void* data, size_t size, Sink& sink);

    uint16_t GetEpoch() const { return m_Epoch; }
    uint16_t GetAckSeq() const { return m_AckSeq; }
    uint64_t GetAckBits() const { return m_AckBits; }

private:
    void Acknowledge(uint16_t seq);

    uint16_t m_Epoch = 0;
    uint16_t m_AckSeq = 0;
    uint64_t m_AckBits = 0; // bit i: m_AckSeq - 1 - i
};
