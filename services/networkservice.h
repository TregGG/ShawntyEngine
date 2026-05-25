#pragma once

#include "service.h"
#include <string>
#include <functional>
#include <enet/enet.h>
#include "../core/network_data.h"

enum class NetworkMode {
    Offline,
    Server,
    Client
};

class NetworkService : public Service {
public:
    NetworkService();
    ~NetworkService() override;

    void Init() override;
    void Update(float dt) override;
    void Shutdown() override;

    bool Host(int port);
    bool Connect(const std::string& ip, int port);
    void Disconnect();

    NetworkMode GetMode() const { return m_Mode; }
    bool IsConnected() const { return m_IsConnected; }
    ENetPeer* GetServerPeer() const { return m_ServerPeer; }
    
    std::function<void()> OnShutdownRequested;
    
    using PeerCallback = std::function<void(ENetPeer* peer)>;
    PeerCallback OnClientDisconnected;
    
    using PacketCallback = std::function<void(ENetPeer* peer, void* data, size_t size)>;
    void SetPacketCallback(PacketCallback cb) { m_PacketCallback = cb; }
    
    // Low level packet sending
    void SendPacket(ENetPeer* peer, enet_uint8 channel, const void* data, size_t size, enet_uint32 flags);
    void BroadcastPacket(enet_uint8 channel, const void* data, size_t size, enet_uint32 flags);

private:
    void PollEvents();

    NetworkMode m_Mode = NetworkMode::Offline;
    ENetHost* m_Host = nullptr;
    ENetPeer* m_ServerPeer = nullptr; // Used if we are a client connecting to a server
    
    ENetPeer* m_AdminPeer = nullptr;
    int m_ConnectedClients = 0;
    float m_EmptyServerTimer = 0.0f;
    float m_AdminDisconnectTimer = -1.0f;
    
    float m_PacketLogTimer = 0.0f;
    int m_PacketsReceivedInLastSecond = 0;
    
    PacketCallback m_PacketCallback;
    bool m_IsConnected = false;
};
