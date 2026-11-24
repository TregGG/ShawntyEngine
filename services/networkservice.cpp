#include "networkservice.h"
#include "../core/logger.h"
#include <iostream>

#define ENGINE_CLASS "NetworkService"
#include "../core/enginedebug.h"

NetworkService::NetworkService() {}
NetworkService::~NetworkService() { Shutdown(); }

void NetworkService::Init() {
    if (enet_initialize() != 0) {
        ENGINE_ERROR("An error occurred while initializing ENet.");
        return;
    }
    ENGINE_LOG("NetworkService Initialized (Offline Mode)");
}

void NetworkService::Shutdown() {
    Disconnect();
    enet_deinitialize();
}

bool NetworkService::Host(int port) {
    if (m_Mode != NetworkMode::Offline) {
        Disconnect();
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = static_cast<enet_uint16>(port);

    m_Host = enet_host_create(&address, 32, 2, 0, 0); // up to 32 clients, 2 channels, no bandwidth limits
    if (m_Host == nullptr) {
        ENGINE_ERROR("An error occurred while trying to create an ENet server host.");
        return false;
    }

    m_Host->maximumPacketSize = 1024 * 1024; // ENet applies this to outgoing packets too
    m_Host->maximumWaitingData = 256 * 1024;  // per client, incoming

    m_Mode = NetworkMode::Server;
    ENGINE_LOG("Started Hosting on port %d", port);
    return true;
}

bool NetworkService::Connect(const std::string& ip, int port) {
    if (m_Mode != NetworkMode::Offline) {
        Disconnect();
    }

    m_Host = enet_host_create(NULL, 1, 2, 0, 0); // 1 connection, 2 channels
    if (m_Host == nullptr) {
        ENGINE_ERROR("An error occurred while trying to create an ENet client host.");
        return false;
    }

    ENetAddress address;
    if (enet_address_set_host(&address, ip.c_str()) != 0) {
        ENGINE_ERROR("Could not resolve server address: %s", ip.c_str());
        Disconnect();
        return false;
    }
    address.port = port;

    m_ServerPeer = enet_host_connect(m_Host, &address, 2, 0);
    if (m_ServerPeer == nullptr) {
        ENGINE_ERROR("No available peers for initiating an ENet connection.");
        Disconnect();
        return false;
    }

    m_Host->maximumPacketSize = 1024 * 1024;

    m_Mode = NetworkMode::Client;
    ENGINE_LOG("Connecting to %s:%d...", ip.c_str(), port);
    return true;
}

void NetworkService::Disconnect() {
    if (m_ServerPeer) {
        enet_peer_disconnect_now(m_ServerPeer, 0);
        m_ServerPeer = nullptr;
    }
    if (m_Host) {
        enet_host_destroy(m_Host);
        m_Host = nullptr;
    }
    m_Mode = NetworkMode::Offline;
    m_IsConnected = false;
}

void NetworkService::Update(float dt) {
    if (m_Mode != NetworkMode::Offline) {
        PollEvents();
        
        m_PacketLogTimer += dt;
        if (m_PacketLogTimer >= 1.0f) {
            if (m_Mode == NetworkMode::Server) {
                ENGINE_LOG("[Server] Packets received in last second: %d", m_PacketsReceivedInLastSecond);
            } else if (m_Mode == NetworkMode::Client) {
                ENGINE_LOG("[Client] Packets received in last second: %d", m_PacketsReceivedInLastSecond);
            }
            m_PacketsReceivedInLastSecond = 0;
            m_PacketLogTimer = 0.0f;
        }
        
        if (m_Mode == NetworkMode::Server) {
            if (m_AdminDisconnectTimer >= 0.0f) {
                m_AdminDisconnectTimer -= dt;
                if (m_AdminDisconnectTimer < 0.0f) {
                    ENGINE_LOG("Admin grace period expired. Auto-shutting down.");
                    if (OnShutdownRequested) OnShutdownRequested();
                }
            }

            if (m_ConnectedClients == 0) {
                m_EmptyServerTimer += dt;
                if (m_EmptyServerTimer >= 300.0f) { // 5 minutes
                    ENGINE_LOG("Server empty for 5 minutes. Auto-shutting down.");
                    if (OnShutdownRequested) OnShutdownRequested();
                }
            } else {
                m_EmptyServerTimer = 0.0f;
            }
        }
    }
}

void NetworkService::SendPacket(ENetPeer* peer, enet_uint8 channel, const void* data, size_t size, enet_uint32 flags) {
    if (!peer || peer->state != ENET_PEER_STATE_CONNECTED) return;
    ENetPacket* packet = enet_packet_create(data, size, flags);
    if (enet_peer_send(peer, channel, packet) < 0) {
        ENGINE_ERROR("Failed to send a packet of %zu bytes", size);
        enet_packet_destroy(packet);
    }
}

void NetworkService::Multicast(const std::vector<ENetPeer*>& peers, enet_uint8 channel, const void* data, size_t size, enet_uint32 flags) {
    if (m_Mode != NetworkMode::Server || !m_Host) return;
    ENetPacket* packet = enet_packet_create(data, size, flags); // shared by all receivers
    for (ENetPeer* peer : peers) {
        if (peer->state == ENET_PEER_STATE_CONNECTED) enet_peer_send(peer, channel, packet);
    }
    if (packet->referenceCount == 0) enet_packet_destroy(packet);
}

void NetworkService::PollEvents() {
    ENetEvent event;
    // timeout of 0 makes it non-blocking
    while (m_Host && enet_host_service(m_Host, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                ENGINE_LOG("A new client connected.");
                if (m_Mode == NetworkMode::Server) {
                    m_ConnectedClients++;
                    if (OnClientConnected) OnClientConnected(event.peer);
                    if (m_AdminPeer == nullptr) {
                        m_AdminPeer = event.peer;
                        m_AdminDisconnectTimer = -1.0f; // Cancel grace period
                        ENGINE_LOG("First client connected! Assigned Admin role.");
                    }
                } else if (m_Mode == NetworkMode::Client) {
                    m_IsConnected = true;
                    if (OnConnected) OnConnected();
                }
                break;
            case ENET_EVENT_TYPE_RECEIVE: {
                m_PacketsReceivedInLastSecond++;
                if (m_PacketCallback) {
                    m_PacketCallback(event.peer, event.packet->data, event.packet->dataLength);
                } else if (event.packet->dataLength >= sizeof(PacketHeader)) {
                    PacketHeader* header = reinterpret_cast<PacketHeader*>(event.packet->data);
                    // We will pass this to NetworkControl or Game later
                    ENGINE_LOG("Received packet of type: %d", (int)header->type);
                }
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                ENGINE_LOG("Client disconnected.");
                if (m_Mode == NetworkMode::Client) {
                    const uint32_t reason = event.data;
                    m_ServerPeer = nullptr;
                    Disconnect(); // destroys the host, so stop polling it
                    if (OnDisconnected) OnDisconnected(reason);
                    return;
                } else if (m_Mode == NetworkMode::Server) {
                    m_ConnectedClients--;
                    if (OnClientDisconnected) {
                        OnClientDisconnected(event.peer);
                    }
                    if (event.peer == m_AdminPeer) {
                        m_AdminPeer = nullptr;
                        if (!m_Persistent) {
                            ENGINE_LOG("Admin client disconnected! Starting 10s grace period...");
                            m_AdminDisconnectTimer = 10.0f;
                        }
                    }
                }
                break;
            case ENET_EVENT_TYPE_NONE:
                break;
        }
    }
}
