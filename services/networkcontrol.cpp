#include "networkcontrol.h"
#include "networkservice.h"
#include "../core/logger.h"
#include "../core/network_data.h"
#include <cstring>

#define ENGINE_CLASS "NetworkControl"
#include "../core/enginedebug.h"

NetworkControl::NetworkControl() {}
NetworkControl::~NetworkControl() {}

void NetworkControl::Init() {
    ENGINE_LOG("NetworkControl Initialized");
}

void NetworkControl::Update(float dt) {
    // Currently no per-frame logic needed for pure control translation
}

void NetworkControl::Shutdown() {
}

void NetworkControl::ProcessCommandString(const std::string& command) {
    if (!m_NetService || command.empty()) return;

    // Example logic: if command starts with "/", it's an admin command
    if (command[0] == '/') {
        AdminCommandPacket packet;
        packet.header.type = PacketType::AdminCommand;
        packet.header.tick = 0; // We could pull tick from a time service
        
        strncpy(packet.command, command.c_str(), sizeof(packet.command) - 1);
        packet.command[sizeof(packet.command) - 1] = '\0'; // ensure null termination

        // Send to server on reliable channel (Channel 1)
        // If we ARE the server, we just process it locally
        if (m_NetService->GetMode() == NetworkMode::Client) {
            // Need to pass it to NetworkService to send to server
            // Since NetworkService hides the peer for simplicity right now, 
            // we'll just broadcast (which sends to server since we only have 1 peer)
            m_NetService->BroadcastPacket(1, &packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);
        } else if (m_NetService->GetMode() == NetworkMode::Server) {
            ENGINE_LOG("Server processing admin command locally: %s", command.c_str());
            // TODO: Execute server logic
        }
    }
}

void NetworkControl::OnPacketReceived(void* data, size_t size) {
    if (size < sizeof(PacketHeader)) return;

    PacketHeader* header = reinterpret_cast<PacketHeader*>(data);

    switch (header->type) {
        case PacketType::AdminCommand: {
            if (size >= sizeof(AdminCommandPacket)) {
                AdminCommandPacket* cmdPacket = reinterpret_cast<AdminCommandPacket*>(data);
                ENGINE_LOG("Received Admin Command: %s", cmdPacket->command);
                // TODO: Verify if sender is host, then execute
            }
            break;
        }
        case PacketType::ServerCommand: {
            if (size >= sizeof(ServerCommandPacket)) {
                ServerCommandPacket* cmdPacket = reinterpret_cast<ServerCommandPacket*>(data);
                ENGINE_LOG("Received Server Command: %s", cmdPacket->command);
                // TODO: Dispatch to SceneManager to load scene
            }
            break;
        }
        // Other types like ClientInput, ServerUpdate, StateSync will be dispatched to Game/ECS
        default:
            break;
    }
}
