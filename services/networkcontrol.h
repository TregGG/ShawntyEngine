#pragma once

#include "service.h"
#include <string>

class NetworkService;

class NetworkControl : public Service {
public:
    NetworkControl();
    ~NetworkControl() override;

    void Init() override;
    void Update(float dt) override;
    void Shutdown() override;

    void SetNetworkService(NetworkService* netService) { m_NetService = netService; }

    // Client/Host calls this when user types in console/UI
    void ProcessCommandString(const std::string& command);

    // NetworkService calls this when an ENet packet is received
    void OnPacketReceived(void* data, size_t size);

private:
    NetworkService* m_NetService = nullptr;
};
