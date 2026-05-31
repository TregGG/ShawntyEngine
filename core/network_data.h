#pragma once

#include <cstdint>

enum class PacketType : uint8_t {
    Connect,
    ClientInput,     // Client -> Server (Raw inputs only: W,A,S,D, mouse delta)
    ServerUpdate,    // Server -> Client (Priority 1/2: Delta ECS Transforms)
    StateSync,       // Server -> Client (Priority 3: Full Verification of all states)
    ActionRequest,   // Client -> Server (Shoot, Interact)
    ActionConfirm,   // Server -> Client (Action validated)
    AdminCommand,    // Client -> Server (e.g., Kick, Stop)
    ServerCommand,   // Server -> Client (e.g., Switch Scene)
    TickSync         // Server -> Client (Reliable tick synchronization)
};

#pragma pack(push, 1)

struct PacketHeader {
    PacketType type;
    uint32_t tick;
};

// Server -> Client: Welcome / Connection handshake
struct ConnectPacket {
    PacketHeader header;
    uint32_t clientEntityID;
};

// Client -> Server: Sent continuously
struct ClientInputPacket {
    PacketHeader header;
    uint16_t inputMask; // bits for W, A, S, D, Space, etc.
    float mouseDeltaX;
    float mouseDeltaY;
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

// The ServerUpdate packets send a PacketHeader, count, and array of EntityVelocityData structs.
struct EntityVelocityData {
    uint32_t entityID;
    float vx;
    float vy;
};

// The StateSync packets send a PacketHeader, count, and array of EntityPositionData structs.
struct EntityPositionData {
    uint32_t entityID;
    float x;
    float y;
};

struct EntityTransformData {
    uint32_t entityID;
    float x, y, z;
    float yaw, pitch, roll;
};

#pragma pack(pop)
