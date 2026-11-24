#pragma once

#include <cstdint>

enum class PacketType : uint8_t {
    Connect,
    ClientInput,     // Client -> Server (Raw inputs only: W,A,S,D, mouse delta)
    ServerUpdate,    // unused, replaced by NetSnapshot
    StateSync,       // unused, replaced by NetSnapshot
    ActionRequest,   // Client -> Server (Shoot, Interact)
    ActionConfirm,   // Server -> Client (Action validated)
    AdminCommand,    // Client -> Server (e.g., Kick, Stop)
    ServerCommand,   // Server -> Client (e.g., Switch Scene)
    TickSync,        // Server -> Client (Reliable tick synchronization)
    // Content sync (see services/contentsync.h). Keep these five together.
    JoinRequest,     // Client -> Server
    ContentManifest, // Server -> Client
    ContentRequest,  // Client -> Server
    ContentChunk,    // Server -> Client
    ContentReady,    // Client -> Server
    // Replication (see services/replication.h)
    NetSnapshot,     // Server -> Client (unreliable)
    NetSpawn,        // Server -> Client (reliable)
    NetDestroy       // Server -> Client (reliable)
};

#pragma pack(push, 1)

struct PacketHeader {
    PacketType type;
    uint32_t tick;
};

// Server -> Client: Welcome / Connection handshake
struct ConnectPacket {
    PacketHeader header;
    uint32_t clientEntityID;   // network ID of the client's own player
    uint32_t sceneEntityCount; // must match the client's scene
    uint16_t epoch;
    char sceneName[64];
};

// Client -> Server: Sent continuously
struct ClientInputPacket {
    static constexpr int kInputs = 4;
    PacketHeader header;          // tick of inputMasks[0]
    uint16_t inputMasks[kInputs]; // newest first; repeats cover lost packets. Bits for W, A, S, D, Space, etc.
    // Acknowledges snapshots
    uint16_t epoch;
    uint16_t ackSeq;
    uint64_t ackBits;
};

// Client -> Server: Sent occasionally
struct AdminCommandPacket {
    PacketHeader header;
    char command[128]; // e.g. "/kick player2"
};

// Server -> Client: Sent occasionally
struct ServerCommandPacket {
    PacketHeader header;
    char command[128]; // e.g. "load_scene levels/level1"
};

struct EntityTransformData {
    uint32_t entityID;
    float x, y, z;
    float yaw, pitch, roll;
};

#pragma pack(pop)
