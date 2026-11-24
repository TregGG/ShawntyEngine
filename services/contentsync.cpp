#include "contentsync.h"
#include "networkservice.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <system_error>
#include <cerrno>
#include <csignal>
#include <unistd.h>

#define ENGINE_CLASS "ContentSync"
#include "../core/enginedebug.h"

namespace fs = std::filesystem;
using namespace ContentLimits;

namespace
{
    class Writer
    {
    public:
        explicit Writer(PacketType type)
        {
            PacketHeader header{};
            header.type = type;
            Raw(&header, sizeof(header));
        }
        void Raw(const void* p, size_t n)
        {
            const auto* b = static_cast<const uint8_t*>(p);
            m_Bytes.insert(m_Bytes.end(), b, b + n);
        }
        template <typename T> void Value(const T& v) { Raw(&v, sizeof(T)); }
        void Send(NetworkService& net, ENetPeer* peer) const
        {
            net.SendPacket(peer, kChannel, m_Bytes.data(), m_Bytes.size(), ENET_PACKET_FLAG_RELIABLE);
        }
    private:
        std::vector<uint8_t> m_Bytes;
    };

    // Every read is bounds checked; after a failed read, Ok() stays false.
    class Reader
    {
    public:
        Reader(const uint8_t* p, size_t n) : m_Pos(p), m_Left(n) {}
        const uint8_t* Raw(size_t n)
        {
            if (!m_Ok || n > m_Left) { m_Ok = false; return nullptr; }
            const uint8_t* p = m_Pos;
            m_Pos += n;
            m_Left -= n;
            return p;
        }
        template <typename T> T Value()
        {
            T v{};
            if (const uint8_t* p = Raw(sizeof(T))) std::memcpy(&v, p, sizeof(T));
            return v;
        }
        bool Ok() const { return m_Ok; }
        size_t Left() const { return m_Left; }
    private:
        const uint8_t* m_Pos;
        size_t m_Left;
        bool m_Ok = true;
    };

    bool ReadFile(const fs::path& path, std::vector<uint8_t>& out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        return !file.bad();
    }

    uint64_t ShortVersion(const Sha256Digest& version)
    {
        uint64_t v;
        std::memcpy(&v, version.data(), sizeof(v));
        return v;
    }

    Sha256Digest HashFileList(const std::vector<ContentFile>& files)
    {
        std::vector<uint8_t> all;
        for (const ContentFile& f : files) {
            all.insert(all.end(), f.path.begin(), f.path.end());
            all.push_back(0);
            all.insert(all.end(), f.hash.begin(), f.hash.end());
        }
        return Sha256(all.data(), all.size());
    }

    bool IsWantedExtension(const std::string& dir, const std::string& ext)
    {
        static const std::map<std::string, std::string> kDirs = {
            {"scenes", ".scene"}, {"textures", ".tga"}, {"spritesheets", ".ssheet"},
            {"animations", ".anim"}, {"objects", ".objasset"}, {"prefabs", ".prefab"}};
        const auto it = kDirs.find(dir);
        return it != kDirs.end() && it->second == ext;
    }
}

const char* DescribeDisconnect(uint32_t reason)
{
    switch (static_cast<DisconnectReason>(reason)) {
        case DisconnectReason::BadJoinCode:  return "Wrong join code";
        case DisconnectReason::WrongVersion: return "Client and server versions differ";
        case DisconnectReason::JoinTimeout:  return "Timed out while joining";
        case DisconnectReason::Misbehaving:  return "Disconnected by the server";
        default:                             return "Connection lost";
    }
}

// "project.json" or "<known dir>/<name><its extension>". Nothing else is ever sent or written.
bool IsSafeContentPath(const std::string& path)
{
    if (path == "project.json") return true;
    const auto slash = path.find('/');
    if (slash == std::string::npos || path.size() > 160) return false;
    const std::string name = path.substr(slash + 1);
    if (name.empty() || name.front() == '.' || name.find("..") != std::string::npos) return false;
    for (const unsigned char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                        || c == '_' || c == '-' || c == '.' || c == ' ';
        if (!ok) return false;
    }
    const auto dot = name.rfind('.');
    return dot != std::string::npos && dot > 0 && IsWantedExtension(path.substr(0, slash), name.substr(dot));
}

// ============================================================
// Server
// ============================================================
bool ContentServer::Load(const std::string& root, std::string& error)
{
    std::vector<ContentFile> files;
    std::vector<std::vector<uint8_t>> data;
    std::vector<std::string> paths;

    std::error_code ec;
    for (const char* dir : {"scenes", "textures", "spritesheets", "animations", "objects", "prefabs"}) {
        for (fs::directory_iterator it(fs::path(root) / dir, ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code entryError;
            if (it->is_regular_file(entryError) && !it->is_symlink(entryError))
                paths.push_back(std::string(dir) + "/" + it->path().filename().string());
        }
        ec.clear();
    }
    if (fs::is_regular_file(fs::path(root) / "project.json", ec)) paths.push_back("project.json");
    std::sort(paths.begin(), paths.end());

    uint64_t total = 0;
    for (const std::string& path : paths) {
        if (!IsSafeContentPath(path)) continue;
        std::vector<uint8_t> bytes;
        const auto size = fs::file_size(fs::path(root) / path, ec);
        if (ec || size > kMaxFileSize) {
            error = path + " is larger than 16 MB";
            return false;
        }
        if (!ReadFile(fs::path(root) / path, bytes)) {
            error = "Could not read " + path;
            return false;
        }
        total += bytes.size();
        if (total > kMaxTotalSize || files.size() >= kMaxFiles) {
            error = "The project is larger than 64 MB or has more than 2000 files";
            return false;
        }
        files.push_back({path, static_cast<uint32_t>(bytes.size()), Sha256(bytes.data(), bytes.size())});
        data.push_back(std::move(bytes));
    }

    m_Files = std::move(files);
    m_Data = std::move(data);
    m_Version = HashFileList(m_Files);
    return true;
}

void ContentServer::OnPeerConnected(ENetPeer* peer) { m_Peers[peer] = PeerState{}; }
void ContentServer::OnPeerDisconnected(ENetPeer* peer) { m_Peers.erase(peer); }

bool ContentServer::IsReady(ENetPeer* peer) const
{
    const auto it = m_Peers.find(peer);
    return it != m_Peers.end() && it->second.joined && it->second.readyVersion == m_Version;
}

void ContentServer::SendManifest(NetworkService& net, ENetPeer* peer) const
{
    Writer w(PacketType::ContentManifest);
    w.Value(m_Version);
    w.Value(static_cast<uint32_t>(m_Files.size()));
    for (const ContentFile& f : m_Files) {
        w.Value(static_cast<uint16_t>(f.path.size()));
        w.Raw(f.path.data(), f.path.size());
        w.Value(f.size);
        w.Value(f.hash);
    }
    w.Send(net, peer);
}

void ContentServer::Announce(NetworkService& net)
{
    for (const auto& [peer, state] : m_Peers)
        if (state.joined) SendManifest(net, peer);
}

void ContentServer::Update(NetworkService&)
{
    const auto now = Clock::now();
    for (auto& [peer, state] : m_Peers) {
        if (!state.joined && now - state.connectedAt > std::chrono::seconds(10)) {
            enet_peer_disconnect_later(peer, static_cast<uint32_t>(DisconnectReason::JoinTimeout));
            state.connectedAt = now;
        }
    }
}

bool ContentServer::OnPacket(NetworkService& net, ENetPeer* peer, const void* data, size_t size)
{
    if (size < sizeof(PacketHeader)) return false;
    const PacketType type = static_cast<const PacketHeader*>(data)->type;
    if (type < PacketType::JoinRequest || type > PacketType::ContentReady) return false;

    const auto it = m_Peers.find(peer);
    if (it == m_Peers.end()) return true;
    PeerState& state = it->second;
    Reader r(static_cast<const uint8_t*>(data) + sizeof(PacketHeader), size - sizeof(PacketHeader));

    if (type == PacketType::JoinRequest) {
        const auto protocol = r.Value<uint8_t>();
        const auto code = r.Value<std::array<char, 16>>();
        if (!r.Ok() || state.joined) return true;
        if (protocol != kProtocol) {
            enet_peer_disconnect_later(peer, static_cast<uint32_t>(DisconnectReason::WrongVersion));
        } else if (!m_JoinCode.empty() && m_JoinCode != std::string(code.data(), strnlen(code.data(), code.size()))) {
            // One attempt per connection, so codes can't be guessed quickly
            enet_peer_disconnect_later(peer, static_cast<uint32_t>(DisconnectReason::BadJoinCode));
            m_Peers.erase(it);
        } else {
            state.joined = true;
            SendManifest(net, peer);
        }
        return true;
    }
    if (!state.joined) return true;

    if (type == PacketType::ContentReady) {
        const auto version = r.Value<Sha256Digest>();
        if (r.Ok()) state.readyVersion = version;
    } else if (type == PacketType::ContentRequest) {
        const auto version = r.Value<uint64_t>();
        const auto index = r.Value<uint32_t>();
        const auto offset = r.Value<uint32_t>();
        if (!r.Ok()) return true;

        // Normal clients can't exceed this: at most kWindow requests per server tick
        const auto now = Clock::now();
        if (now - state.windowStart > std::chrono::seconds(1)) {
            state.windowStart = now;
            state.requestsInWindow = 0;
        }
        if (++state.requestsInWindow > 4000) {
            enet_peer_disconnect_later(peer, static_cast<uint32_t>(DisconnectReason::Misbehaving));
            m_Peers.erase(it);
            return true;
        }
        if (version != ShortVersion(m_Version) || index >= m_Data.size() || offset >= m_Data[index].size()) return true;

        const uint32_t length = std::min<uint32_t>(kChunkSize, static_cast<uint32_t>(m_Data[index].size()) - offset);
        Writer w(PacketType::ContentChunk);
        w.Value(version);
        w.Value(index);
        w.Value(offset);
        w.Raw(m_Data[index].data() + offset, length);
        w.Send(net, peer);
    }
    return true;
}

// ============================================================
// Client
// ============================================================
fs::path ContentClient::DefaultCacheDir()
{
    if (const char* dir = std::getenv("XDG_CACHE_HOME"); dir && *dir) return fs::path(dir) / "shawnty";
    if (const char* home = std::getenv("HOME"); home && *home) return fs::path(home) / ".cache" / "shawnty";
    return fs::temp_directory_path() / "shawnty-cache";
}

ContentClient::ContentClient(fs::path cacheDir) : m_CacheDir(std::move(cacheDir)) {}
ContentClient::~ContentClient() { RemoveProjectDir(); }

void ContentClient::RemoveProjectDir()
{
    std::error_code ec;
    if (!m_ProjectDir.empty()) fs::remove_all(m_ProjectDir, ec);
    m_ProjectDir.clear();
}

void ContentClient::OnConnected(NetworkService& net)
{
    OnDisconnected();
    std::array<char, 16> code{};
    std::memcpy(code.data(), m_JoinCode.data(), std::min(m_JoinCode.size(), code.size()));
    Writer w(PacketType::JoinRequest);
    w.Value(kProtocol);
    w.Value(code);
    w.Send(net, net.GetServerPeer());
}

void ContentClient::OnDisconnected()
{
    m_Ready = false;
    m_Files.clear();
    m_Missing.clear();
    m_Buffer.clear();
    m_Requested = m_Received = 0;
}

bool ContentClient::OnPacket(NetworkService& net, const void* data, size_t size)
{
    if (size < sizeof(PacketHeader)) return false;
    const PacketType type = static_cast<const PacketHeader*>(data)->type;
    const uint8_t* body = static_cast<const uint8_t*>(data) + sizeof(PacketHeader);
    if (type == PacketType::ContentManifest) OnManifest(net, body, size - sizeof(PacketHeader));
    else if (type == PacketType::ContentChunk) OnChunk(net, body, size - sizeof(PacketHeader));
    else return false;
    return true;
}

void ContentClient::Fail(NetworkService& net, const std::string& error)
{
    ENGINE_ERROR("%s", error.c_str());
    OnDisconnected();
    if (OnError) OnError(error);
    if (net.GetServerPeer()) enet_peer_disconnect_later(net.GetServerPeer(), 0);
}

bool ContentClient::HasBlob(const ContentFile& file) const
{
    std::error_code ec;
    const fs::path path = BlobPath(file.hash);
    if (!fs::is_regular_file(path, ec) || fs::file_size(path, ec) != file.size) return false;
    std::vector<uint8_t> bytes;
    return ReadFile(path, bytes) && Sha256(bytes.data(), bytes.size()) == file.hash;
}

void ContentClient::OnManifest(NetworkService& net, const uint8_t* data, size_t size)
{
    Reader r(data, size);
    const auto version = r.Value<Sha256Digest>();
    const auto count = r.Value<uint32_t>();
    if (!r.Ok() || count > kMaxFiles) return Fail(net, "Invalid content list from server");

    std::vector<ContentFile> files;
    std::set<std::string> seen;
    uint64_t total = 0;
    for (uint32_t i = 0; i < count; ++i) {
        ContentFile f;
        const auto length = r.Value<uint16_t>();
        if (const uint8_t* p = r.Raw(length)) f.path.assign(reinterpret_cast<const char*>(p), length);
        f.size = r.Value<uint32_t>();
        f.hash = r.Value<Sha256Digest>();
        total += f.size;
        if (!r.Ok() || !IsSafeContentPath(f.path) || f.size > kMaxFileSize || total > kMaxTotalSize
            || !seen.insert(f.path).second)
            return Fail(net, "Invalid content list from server");
        files.push_back(std::move(f));
    }
    if (HashFileList(files) != version) return Fail(net, "Invalid content list from server");

    OnDisconnected();
    m_Files = std::move(files);
    m_Version = version;
    m_Downloaded = 0;
    if (OnUpdating) OnUpdating();

    std::set<Sha256Digest> queued;
    for (uint32_t i = 0; i < m_Files.size(); ++i)
        if (!HasBlob(m_Files[i]) && queued.insert(m_Files[i].hash).second) m_Missing.push_back(i);
    m_TotalMissing = m_Missing.size();
    NextFile(net);
}

void ContentClient::NextFile(NetworkService& net)
{
    for (;;) {
        if (OnProgress) OnProgress(m_Downloaded, m_TotalMissing);
        if (m_Missing.empty()) {
            Finish();
            return;
        }
        const ContentFile& file = m_Files[m_Missing.back()];
        m_Buffer.assign(file.size, 0);
        m_Requested = m_Received = 0;
        if (file.size > 0) break;

        // Empty files need no request
        std::error_code ec;
        fs::create_directories(m_CacheDir / "blobs", ec);
        std::ofstream(BlobPath(file.hash), std::ios::binary);
        if (Sha256(nullptr, 0) != file.hash) return Fail(net, "Corrupt file from server: " + file.path);
        m_Missing.pop_back();
        ++m_Downloaded;
    }
    Pump(net);
}

void ContentClient::Pump(NetworkService& net)
{
    const uint32_t size = static_cast<uint32_t>(m_Buffer.size());
    while (m_Requested < size && m_Requested - m_Received < kWindow * kChunkSize) {
        Writer w(PacketType::ContentRequest);
        w.Value(ShortVersion(m_Version));
        w.Value(m_Missing.back());
        w.Value(m_Requested);
        w.Send(net, net.GetServerPeer());
        m_Requested += std::min(kChunkSize, size - m_Requested);
    }
}

void ContentClient::OnChunk(NetworkService& net, const uint8_t* data, size_t size)
{
    Reader r(data, size);
    const auto version = r.Value<uint64_t>();
    const auto index = r.Value<uint32_t>();
    const auto offset = r.Value<uint32_t>();
    if (!r.Ok()) return Fail(net, "Invalid data from server");
    if (m_Missing.empty() || version != ShortVersion(m_Version)) return; // left over from an older version

    // The channel is ordered, so chunks arrive exactly as requested
    const uint32_t remaining = static_cast<uint32_t>(m_Buffer.size()) - m_Received;
    const uint32_t expected = std::min(kChunkSize, remaining);
    if (index != m_Missing.back() || offset != m_Received || r.Left() != expected || m_Received >= m_Requested)
        return Fail(net, "Invalid data from server");
    std::memcpy(m_Buffer.data() + offset, r.Raw(expected), expected);
    m_Received += expected;

    if (m_Received < m_Buffer.size()) {
        Pump(net);
        return;
    }

    const ContentFile& file = m_Files[index];
    if (Sha256(m_Buffer.data(), m_Buffer.size()) != file.hash)
        return Fail(net, "Corrupt file from server: " + file.path);

    std::error_code ec;
    fs::create_directories(m_CacheDir / "blobs", ec);
    const fs::path temp = BlobPath(file.hash).string() + "." + std::to_string(getpid());
    {
        std::ofstream out(temp, std::ios::binary);
        out.write(reinterpret_cast<const char*>(m_Buffer.data()), static_cast<std::streamsize>(m_Buffer.size()));
        out.close();
        if (!out) return Fail(net, "Could not write to the cache: " + temp.string());
    }
    fs::rename(temp, BlobPath(file.hash), ec);
    if (ec) return Fail(net, "Could not write to the cache: " + ec.message());

    m_Missing.pop_back();
    ++m_Downloaded;
    NextFile(net);
}

void ContentClient::Finish()
{
    // Each process and version has its own folder, so nothing in use is ever modified
    const fs::path previous = m_ProjectDir;
    const fs::path dir = m_CacheDir / "projects"
        / (ToHex(m_Version).substr(0, 16) + "-" + std::to_string(getpid()) + "-" + std::to_string(++m_Generation));
    std::error_code ec;
    fs::remove_all(dir, ec);
    for (const char* sub : {"scenes", "textures", "spritesheets", "animations", "objects", "prefabs"})
        fs::create_directories(dir / sub, ec);
    for (const ContentFile& f : m_Files) {
        const fs::path target = dir / f.path;
        fs::copy_file(BlobPath(f.hash), target, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            if (OnError) OnError("Could not write " + target.string());
            return;
        }
    }
    m_ProjectDir = dir;
    PruneCache();
    if (OnContent) OnContent(dir.string());
    if (!previous.empty()) fs::remove_all(previous, ec);
}

// Keeps the downloaded files under 512 MB, dropping the oldest.
void ContentClient::PruneCache() const
{
    // Project copies left behind by clients that crashed
    std::error_code dirError;
    for (fs::directory_iterator it(m_CacheDir / "projects", dirError), end; !dirError && it != end; it.increment(dirError)) {
        const std::string name = it->path().filename().string();
        const auto dash = name.find('-');
        const int pid = dash == std::string::npos ? 0 : std::atoi(name.c_str() + dash + 1);
        std::error_code e;
        if (pid > 0 && kill(pid, 0) != 0 && errno == ESRCH) fs::remove_all(it->path(), e);
    }

    std::set<std::string> inUse;
    for (const ContentFile& f : m_Files) inUse.insert(ToHex(f.hash));

    struct Blob { fs::file_time_type time; uintmax_t size; fs::path path; };
    std::vector<Blob> blobs;
    uintmax_t total = 0;
    std::error_code ec;
    for (fs::directory_iterator it(m_CacheDir / "blobs", ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code e;
        const auto size = it->file_size(e);
        const auto time = it->last_write_time(e);
        if (e) continue;
        total += size;
        blobs.push_back({time, size, it->path()});
    }
    std::sort(blobs.begin(), blobs.end(), [](const Blob& a, const Blob& b) { return a.time < b.time; });
    for (const Blob& b : blobs) {
        if (total <= (512u << 20)) break;
        if (inUse.count(b.path.filename().string())) continue;
        fs::remove(b.path, ec);
        total -= b.size;
    }
}

void ContentClient::SendReady(NetworkService& net)
{
    m_Ready = true;
    Writer w(PacketType::ContentReady);
    w.Value(m_Version);
    w.Send(net, net.GetServerPeer());
}
