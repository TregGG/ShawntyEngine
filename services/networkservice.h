#pragma once

#include "service.h"
#include <string>
#include <vector>
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
    
    bool IsPlayerPushingPrevented() const { return m_PreventPlayerPushing; }
    void SetPreventPlayerPushing(bool prevent) { m_PreventPlayerPushing = prevent; }
    
    bool IsPlayersTransparent() const { return m_PlayersTransparent; }
    void SetPlayersTransparent(bool transparent) { m_PlayersTransparent = transparent; }
    
    void SetPersistent(bool persistent) { m_Persistent = persistent; }

    std::function<void()> OnShutdownRequested;
    
    using PeerCallback = std::function<void(ENetPeer* peer)>;
    PeerCallback OnClientConnected;
    PeerCallback OnClientDisconnected;

    // Client side
    std::function<void()> OnConnected;
    std::function<void(uint32_t reason)> OnDisconnected;
    
    using PacketCallback = std::function<void(ENetPeer* peer, void* data, size_t size)>;
    void SetPacketCallback(PacketCallback cb) { m_PacketCallback = cb; }
    
    // Low level packet sending
    void SendPacket(ENetPeer* peer, enet_uint8 channel, const void* data, size_t size, enet_uint32 flags);
    void Multicast(const std::vector<ENetPeer*>& peers, enet_uint8 channel, const void* data, size_t size, enet_uint32 flags);
    // Sends everything queued now instead of on the next Update
    void Flush() { if (m_Host) enet_host_flush(m_Host); }

private:
    void PollEvents();

    NetworkMode m_Mode = NetworkMode::Offline;
    ENetHost* m_Host = nullptr;
    ENetPeer* m_ServerPeer = nullptr; // Used if we are a client connecting to a server
    
    ENetPeer* m_AdminPeer = nullptr;
    int m_ConnectedClients = 0;
    float m_EmptyServerTimer = 0.0f;
    float m_AdminDisconnectTimer = -1.0f;
    
    bool m_Persistent = false;
    float m_PacketLogTimer = 0.0f;
    int m_PacketsReceivedInLastSecond = 0;
    
    PacketCallback m_PacketCallback;
    bool m_IsConnected = false;
    bool m_PreventPlayerPushing = true;
    bool m_PlayersTransparent = false;
};
