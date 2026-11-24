#include "probe.h"
#include "endpoint.h"
#include "../services/contentsync.h"
#include "../services/clientclock.h"
#include "../services/replication.h"
#include "../services/networkservice.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <thread>

namespace
{
    struct PrintSink : ReplicationClient::Sink
    {
        std::set<uint32_t> known;
        std::map<uint32_t, float> lastX;
        float ownX = -1e9f;

        bool OnNetKnows(uint32_t id) override { return known.count(id) > 0; }
        void OnNetSpawn(const NetSpawn& s, uint32_t) override
        {
            known.insert(s.netId);
            std::printf("spawn: id=%u kind=%s name=%s sprite=%s\n", s.netId, s.kind == NetSpawn::kPlayer ? "player" : "entity",
                        s.name.c_str(), s.state.look.sprite.c_str());
        }
        void OnNetDestroy(uint32_t id) override
        {
            known.erase(id);
            std::printf("destroy: id=%u\n", id);
        }
        void OnNetMotion(uint32_t id, const NetMotion& m, uint32_t) override
        {
            const auto it = lastX.find(id);
            const float x = m.Position().x;
            if (it != lastX.end() && std::abs(x - it->second) < 0.5f) return;
            lastX[id] = x;
            std::printf("entity: id=%u x=%.2f y=%.2f vx=%.2f\n", id, x, m.Position().y, m.Velocity().x);
        }
        void OnNetLook(uint32_t id, const NetLook& l) override
        {
            std::printf("look: id=%u size=%.2f sprite=%s clip=%s\n", id, l.width / 256.0f, l.sprite.c_str(), l.clip.c_str());
        }
        void OnNetOwnPosition(const glm::vec2& p, uint32_t) override
        {
            if (std::abs(p.x - ownX) > 0.5f) std::printf("position: x=%.1f\n", ownX = p.x);
        }
    };
}

int RunProbe(const std::string& address, int seconds)
{
    std::string host, code;
    int port = 0;
    if (!ParseEndpoint(address, host, port, code)) {
        std::fprintf(stderr, "Use host:port or host:port/CODE\n");
        return 2;
    }

    NetworkService net;
    ContentClient content;
    bool disconnected = false;
    ClientClock clock;
    ReplicationClient replication;
    PrintSink sink;
    bool welcomed = false;
    uint32_t minLead = ~0u, maxLead = 0;
    uint16_t recent[ClientInputPacket::kInputs] = {};
    content.SetJoinCode(code);
    net.Init();
    net.OnConnected = [&] { content.OnConnected(net); };
    net.OnDisconnected = [&](uint32_t reason) {
        std::printf("disconnected: %s\n", DescribeDisconnect(reason));
        disconnected = true;
    };
    content.OnError = [](const std::string& e) { std::printf("error: %s\n", e.c_str()); };
    content.OnContent = [&](const std::string& root) {
        std::printf("content: files=%zu downloaded=%zu root=%s\n", content.FileCount(), content.DownloadedCount(), root.c_str());
        content.SendReady(net);
    };
    net.SetPacketCallback([&](ENetPeer*, void* data, size_t size) {
        if (content.OnPacket(net, data, size)) return;
        if (size < sizeof(PacketHeader)) return;
        const auto* header = static_cast<const PacketHeader*>(data);
        if (welcomed && (header->type == PacketType::TickSync || header->type == PacketType::NetSnapshot)) {
            clock.OnServerTick(header->tick, net.GetServerPeer());
            minLead = std::min(minLead, clock.GetLead());
            maxLead = std::max(maxLead, clock.GetLead());
        }
        if (header->type == PacketType::Connect && size >= sizeof(ConnectPacket)) {
            ConnectPacket packet = *static_cast<const ConnectPacket*>(data);
            packet.sceneName[sizeof(packet.sceneName) - 1] = '\0';
            clock.Start(packet.header.tick, net.GetServerPeer());
            welcomed = true;
            replication.Reset(packet.epoch);
            sink.known.clear();
            for (uint32_t i = 1; i <= std::min(packet.sceneEntityCount, 100000u); ++i) sink.known.insert(i);
            sink.known.insert(packet.clientEntityID);
            std::printf("welcome: scene=%s\n", packet.sceneName);
        }
        if (welcomed) replication.OnPacket(data, size, sink);
    });
    if (!net.Connect(host, port)) return 1;

    auto nextTick = std::chrono::steady_clock::now();
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (!disconnected && std::chrono::steady_clock::now() < end) {
        net.Update(0.016f);
        const auto now = std::chrono::steady_clock::now();
        if (now - nextTick > std::chrono::milliseconds(100)) nextTick = now; // after a stall
        while (content.IsReady() && now >= nextTick) {
            nextTick += std::chrono::microseconds(16667);
            for (int i = clock.TicksToRun(); i > 0; --i) {
                clock.Advance();
                std::copy_backward(recent, recent + ClientInputPacket::kInputs - 1, recent + ClientInputPacket::kInputs);
                recent[0] = 8; // move right
                ClientInputPacket input{};
                input.header.type = PacketType::ClientInput;
                input.header.tick = clock.GetTick();
                std::memcpy(input.inputMasks, recent, sizeof(recent));
                input.epoch = replication.GetEpoch();
                input.ackSeq = replication.GetAckSeq();
                input.ackBits = replication.GetAckBits();
                net.SendPacket(net.GetServerPeer(), 0, &input, sizeof(input), 0);
            }
        }
        net.Flush();
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    if (maxLead > 0) std::printf("lead: %u to %u ticks\n", minLead, maxLead);
    net.Shutdown();
    return disconnected ? 1 : 0;
}
