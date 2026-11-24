#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <enet/enet.h>
#include "../core/network_data.h"
#include "../core/sha256.h"

class NetworkService;

// Sends a project's data files (never scripts) from the server to clients.
// Content uses its own reliable channel so it doesn't delay gameplay packets.
namespace ContentLimits
{
    constexpr uint8_t  kProtocol = 3;
    constexpr uint8_t  kChannel = 1;
    constexpr uint32_t kMaxFiles = 2000;
    constexpr uint32_t kMaxFileSize = 16u << 20;
    constexpr uint64_t kMaxTotalSize = 64u << 20;
    constexpr uint32_t kChunkSize = 16u << 10;
    constexpr uint32_t kWindow = 32;
}

enum class DisconnectReason : uint32_t { None = 0, BadJoinCode = 1, WrongVersion = 2, JoinTimeout = 3, Misbehaving = 4 };
const char* DescribeDisconnect(uint32_t reason);

bool IsSafeContentPath(const std::string& path);

struct ContentFile
{
    std::string path;
    uint32_t size = 0;
    Sha256Digest hash{};
};

class ContentServer
{
public:
    // Snapshots the project's files in memory. Returns false, changing nothing, on error.
    bool Load(const std::string& root, std::string& error);
    void SetJoinCode(const std::string& code) { m_JoinCode = code; }
    const Sha256Digest& GetVersion() const { return m_Version; }

    void OnPeerConnected(ENetPeer* peer);
    void OnPeerDisconnected(ENetPeer* peer);
    // True if the packet was a content packet (and has been handled).
    bool OnPacket(NetworkService& net, ENetPeer* peer, const void* data, size_t size);
    // The peer has joined and loaded the current version.
    bool IsReady(ENetPeer* peer) const;
    // Tells joined clients about the current version.
    void Announce(NetworkService& net);
    void Update(NetworkService& net);

private:
    using Clock = std::chrono::steady_clock;
    struct PeerState
    {
        bool joined = false;
        Sha256Digest readyVersion{};
        Clock::time_point connectedAt = Clock::now();
        Clock::time_point windowStart = Clock::now();
        uint32_t requestsInWindow = 0;
    };

    void SendManifest(NetworkService& net, ENetPeer* peer) const;

    std::string m_JoinCode;
    std::vector<ContentFile> m_Files;
    std::vector<std::vector<uint8_t>> m_Data;
    Sha256Digest m_Version{};
    std::map<ENetPeer*, PeerState> m_Peers;
};

class ContentClient
{
public:
    explicit ContentClient(std::filesystem::path cacheDir = DefaultCacheDir());
    ~ContentClient();

    static std::filesystem::path DefaultCacheDir();

    void SetJoinCode(const std::string& code) { m_JoinCode = code; }
    void OnConnected(NetworkService& net);
    void OnDisconnected();
    bool OnPacket(NetworkService& net, const void* data, size_t size);

    // Call after the game has switched to the new content.
    void SendReady(NetworkService& net);
    bool IsReady() const { return m_Ready; }
    size_t FileCount() const { return m_Files.size(); }
    size_t DownloadedCount() const { return m_Downloaded; }

    std::function<void()> OnUpdating;                             // a new version is being fetched
    std::function<void(size_t done, size_t total)> OnProgress;
    std::function<void(const std::string& root)> OnContent;       // everything is on disk
    std::function<void(const std::string& error)> OnError;

private:
    void OnManifest(NetworkService& net, const uint8_t* data, size_t size);
    void OnChunk(NetworkService& net, const uint8_t* data, size_t size);
    void Pump(NetworkService& net);
    void NextFile(NetworkService& net);
    void Finish();
    void Fail(NetworkService& net, const std::string& error);
    void RemoveProjectDir();
    void PruneCache() const;
    std::filesystem::path BlobPath(const Sha256Digest& hash) const { return m_CacheDir / "blobs" / ToHex(hash); }
    bool HasBlob(const ContentFile& file) const;

    std::filesystem::path m_CacheDir;
    std::filesystem::path m_ProjectDir;
    std::string m_JoinCode;
    std::vector<ContentFile> m_Files;
    std::vector<uint32_t> m_Missing;   // indexes into m_Files, downloaded from the back
    size_t m_TotalMissing = 0;
    size_t m_Downloaded = 0;
    Sha256Digest m_Version{};
    bool m_Ready = false;

    std::vector<uint8_t> m_Buffer;
    uint32_t m_Requested = 0;
    uint32_t m_Received = 0;
    uint32_t m_Generation = 0;
};
