#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <enet/enet.h>

// Keeps a client far enough ahead of the server that its inputs arrive before they are needed:
// a full round trip plus room for jitter. The lead follows the connection as it changes, by
// running an extra tick or skipping one now and then instead of jumping.
class ClientClock
{
public:
    uint32_t GetTick() const { return m_Tick; }
    uint32_t GetLead() const { return m_Lead; }

    void Reset() { *this = ClientClock(); }

    void Start(uint32_t serverTick, const ENetPeer* peer)
    {
        m_Lead = LeadFor(peer);
        m_Tick = serverTick + m_Lead;
        m_Drift = 0.0f;
        m_SinceAdjust = 0;
    }

    // Call for every packet carrying a server tick. True if the clock had to jump.
    bool OnServerTick(uint32_t serverTick, const ENetPeer* peer)
    {
        m_Lead = LeadFor(peer);
        // Unsigned subtraction wraps safely for any tick a server might send
        const int64_t diff = static_cast<int32_t>(m_Tick - (serverTick + m_Lead));
        if (diff > kMaxDrift || diff < -kMaxDrift) {
            Start(serverTick, peer);
            return true;
        }
        m_Drift += (static_cast<float>(diff) - m_Drift) * 0.2f;
        return false;
    }

    // How many ticks to run now: usually 1, sometimes 0 or 2 to correct drift
    int TicksToRun()
    {
        if (++m_SinceAdjust < kAdjustInterval || std::abs(m_Drift) < 1.0f) return 1;
        m_SinceAdjust = 0;
        if (m_Drift > 0) { m_Drift -= 1.0f; return 0; }
        m_Drift += 1.0f;
        return 2;
    }

    void Advance() { ++m_Tick; }

private:
    static constexpr float kTickMs = 1000.0f / 60.0f;
    static constexpr int kMaxDrift = 12;
    static constexpr int kAdjustInterval = 12;

    static uint32_t LeadFor(const ENetPeer* peer)
    {
        if (!peer) return 5;
        const float ms = peer->roundTripTime + 2.0f * peer->roundTripTimeVariance;
        return std::min<uint32_t>(static_cast<uint32_t>(std::ceil(ms / kTickMs)) + 3, 90);
    }

    uint32_t m_Tick = 0;
    uint32_t m_Lead = 5;
    float m_Drift = 0.0f;
    int m_SinceAdjust = 0;
};
