#include "replication.h"
#include "networkservice.h"
#include "base/entityregistry/entityregistry.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#define ENGINE_CLASS "Replication"
#include "../core/enginedebug.h"

namespace
{
    constexpr size_t kMaxString = 64;
    constexpr uint8_t kSnapshotChannel = 2;
    constexpr float kPi = 3.14159265358979f;

    int32_t ToFixed(float v, float scale)
    {
        if (!std::isfinite(v)) return 0;
        return static_cast<int32_t>(std::lround(std::clamp(v * scale, -1.0e9f, 1.0e9f)));
    }

    class Out
    {
    public:
        void Raw(const void* p, size_t n)
        {
            const auto* b = static_cast<const uint8_t*>(p);
            m_Bytes.insert(m_Bytes.end(), b, b + n);
        }
        template <typename T> void Value(const T& v) { Raw(&v, sizeof(T)); }
        void Unsigned(uint32_t v)
        {
            for (; v >= 0x80; v >>= 7) m_Bytes.push_back(static_cast<uint8_t>(v | 0x80));
            m_Bytes.push_back(static_cast<uint8_t>(v));
        }
        void Signed(int32_t v) { Unsigned((static_cast<uint32_t>(v) << 1) ^ static_cast<uint32_t>(v >> 31)); }
        void String(const std::string& s)
        {
            const size_t n = std::min(s.size(), kMaxString);
            m_Bytes.push_back(static_cast<uint8_t>(n));
            Raw(s.data(), n);
        }
        void Header(PacketType type, uint32_t tick)
        {
            PacketHeader h;
            h.type = type;
            h.tick = tick;
            Value(h);
        }
        void Clear() { m_Bytes.clear(); }
        size_t Size() const { return m_Bytes.size(); }
        void Append(const Out& o) { Raw(o.m_Bytes.data(), o.m_Bytes.size()); }
        const std::vector<uint8_t>& Bytes() const { return m_Bytes; }
        uint8_t& At(size_t i) { return m_Bytes[i]; }
    private:
        std::vector<uint8_t> m_Bytes;
    };

    // After a bad read, Ok() stays false and every value is zero.
    class In
    {
    public:
        In(const void* p, size_t n) : m_Pos(static_cast<const uint8_t*>(p)), m_Left(n) {}
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
        uint32_t Unsigned()
        {
            uint32_t v = 0;
            for (int shift = 0; shift < 35; shift += 7) {
                const uint8_t* p = Raw(1);
                if (!p) return 0;
                if (shift == 28 && *p > 0x0F) break; // more than 32 bits
                v |= static_cast<uint32_t>(*p & 0x7F) << shift;
                if (!(*p & 0x80)) return v;
            }
            m_Ok = false;
            return 0;
        }
        int32_t Signed()
        {
            const uint32_t v = Unsigned();
            return static_cast<int32_t>((v >> 1) ^ (~(v & 1) + 1));
        }
        std::string String()
        {
            const uint8_t* n = Raw(1);
            if (!n || *n > kMaxString) { m_Ok = false; return {}; }
            const size_t length = *n;
            const uint8_t* p = Raw(length);
            return p ? std::string(reinterpret_cast<const char*>(p), length) : std::string();
        }
        float Float()
        {
            const float v = Value<float>();
            if (!std::isfinite(v)) m_Ok = false;
            return m_Ok ? v : 0.0f;
        }
        void Fail() { m_Ok = false; }
        bool Ok() const { return m_Ok; }
        bool Empty() const { return m_Left == 0; }
    private:
        const uint8_t* m_Pos;
        size_t m_Left;
        bool m_Ok = true;
    };

    void Write(Out& o, const NetMotion& m)
    {
        o.Signed(m.x);
        o.Signed(m.y);
        // Most things don't rotate and many don't move
        const uint8_t flags = (m.vx || m.vy ? 1 : 0) | (m.rotation ? 2 : 0);
        o.Value(flags);
        if (flags & 1) { o.Signed(m.vx); o.Signed(m.vy); }
        if (flags & 2) o.Value(m.rotation);
    }

    NetMotion ReadMotion(In& in)
    {
        NetMotion m;
        m.x = in.Signed();
        m.y = in.Signed();
        const auto flags = in.Value<uint8_t>();
        if (flags > 3) in.Fail();
        if (flags & 1) { m.vx = in.Signed(); m.vy = in.Signed(); }
        if (flags & 2) m.rotation = in.Value<int16_t>();
        return m;
    }

    void Write(Out& o, const NetLook& l)
    {
        o.Value(l.flags);
        o.Value(l.layer);
        o.Signed(l.width);
        o.Signed(l.height);
        if (l.flags & NetLook::kSprite) { o.String(l.sprite); o.Signed(l.frame); }
        if (l.flags & NetLook::kAnimator) {
            o.Value(l.animCommand);
            o.Signed(l.animSpeed);
            o.String(l.animator);
            o.String(l.clip);
        }
    }

    NetLook ReadLook(In& in)
    {
        NetLook l;
        l.flags = in.Value<uint8_t>();
        l.layer = in.Value<uint8_t>();
        if (l.flags > 127 || l.layer >= static_cast<uint8_t>(Layer::Count)) in.Fail();
        l.width = in.Signed();
        l.height = in.Signed();
        if (l.flags & NetLook::kSprite) { l.sprite = in.String(); l.frame = in.Signed(); }
        if (l.flags & NetLook::kAnimator) {
            l.animCommand = in.Value<uint8_t>();
            l.animSpeed = in.Signed();
            l.animator = in.String();
            l.clip = in.String();
        }
        return l;
    }

    void Write(Out& o, const NetSpawn& s)
    {
        o.Unsigned(s.netId);
        o.Value(s.kind);
        o.Value(s.category);
        o.String(s.name);
        Write(o, s.state.motion);
        Write(o, s.state.look);
        if (s.state.look.flags & NetLook::kCollider) {
            o.Value(static_cast<uint8_t>(s.autoBounds));
            o.Value(s.colliderOffset.x);
            o.Value(s.colliderOffset.y);
            o.Value(s.colliderSize.x);
            o.Value(s.colliderSize.y);
            o.Value(s.layerMask);
        }
    }

    NetSpawn ReadSpawn(In& in)
    {
        NetSpawn s;
        s.netId = in.Unsigned();
        s.kind = in.Value<uint8_t>();
        s.category = in.Value<uint8_t>();
        if (s.kind > NetSpawn::kGeneric || s.category >= static_cast<uint8_t>(EntityCategory::Count)) in.Fail();
        s.name = in.String();
        s.state.motion = ReadMotion(in);
        s.state.look = ReadLook(in);
        if (s.state.look.flags & NetLook::kCollider) {
            s.autoBounds = in.Value<uint8_t>() != 0;
            s.colliderOffset.x = in.Float();
            s.colliderOffset.y = in.Float();
            s.colliderSize.x = in.Float();
            s.colliderSize.y = in.Float();
            s.layerMask = in.Value<uint32_t>();
        }
        return s;
    }

    void SendReliable(NetworkService& net, ENetPeer* peer, const Out& o)
    {
        net.SendPacket(peer, 0, o.Bytes().data(), o.Size(), ENET_PACKET_FLAG_RELIABLE);
    }
}

// ============================================================
// State
// ============================================================
glm::vec2 NetMotion::Position() const { return {x / 256.0f, y / 256.0f}; }
glm::vec2 NetMotion::Velocity() const { return {vx / 64.0f, vy / 64.0f}; }
float NetMotion::Rotation() const { return rotation * kPi / 32767.0f; }
void NetMotion::SetPosition(const glm::vec2& p) { x = ToFixed(p.x, 256.0f); y = ToFixed(p.y, 256.0f); }
void NetMotion::SetVelocity(const glm::vec2& v) { vx = ToFixed(v.x, 64.0f); vy = ToFixed(v.y, 64.0f); }
void NetMotion::SetRotation(float radians)
{
    if (!std::isfinite(radians)) radians = 0.0f;
    rotation = static_cast<int16_t>(std::lround(std::remainder(radians, 2.0f * kPi) * 32767.0f / kPi));
}

NetState CaptureNetState(const EntityRegistryService& registry, EntityID e, bool localSpace)
{
    NetState s;
    if (registry.HasComponent<TransformComponent>(e)) {
        const auto& t = registry.GetComponent<TransformComponent>(e);
        s.motion.SetPosition(localSpace && t.parentTransform ? t.localPosition : t.GetWorldPosition());
        s.motion.SetRotation(t.rotation);
        s.look.width = ToFixed(t.size.x, 256.0f);
        s.look.height = ToFixed(t.size.y, 256.0f);
    }
    if (registry.HasComponent<RigidBodyComponent>(e)) {
        const auto& rb = registry.GetComponent<RigidBodyComponent>(e);
        if (rb.GetType() != BodyType::Static) {
            s.look.flags |= NetLook::kBody;
            s.motion.SetVelocity(rb.GetVelocity());
        }
    }
    if (registry.HasComponent<ColliderComponent>(e)) {
        s.look.flags |= NetLook::kCollider;
        if (registry.GetComponent<ColliderComponent>(e).IsTrigger()) s.look.flags |= NetLook::kTrigger;
    }
    if (registry.HasComponent<SpriteComponent2D>(e)) {
        const auto& sprite = registry.GetComponent<SpriteComponent2D>(e);
        s.look.flags |= NetLook::kSprite;
        s.look.sprite = sprite.objectId.substr(0, kMaxString);
        s.look.frame = sprite.frameIndex;
        s.look.layer = static_cast<uint8_t>(sprite.layer);
    }
    if (registry.HasComponent<AnimatorComponent>(e)) {
        const auto& a = registry.GetComponent<AnimatorComponent>(e);
        s.look.flags |= NetLook::kAnimator;
        if (a.GetLoop()) s.look.flags |= NetLook::kAnimLoop;
        if (a.IsStopped()) s.look.flags |= NetLook::kAnimStopped;
        s.look.animCommand = a.GetCommandCount();
        s.look.animSpeed = ToFixed(a.GetSpeed(), 64.0f);
        s.look.animator = a.objectId.substr(0, kMaxString);
        s.look.clip = a.GetClipName().substr(0, kMaxString);
    }
    return s;
}

// ============================================================
// Server
// ============================================================
void ReplicationServer::BeginScene(EntityRegistryService& registry, const std::vector<EntityID>& sceneEntities,
                                   const std::vector<NetState>& baseline)
{
    m_Registry = &registry;
    ++m_Epoch;
    m_Entries.clear();
    m_FreeSlots.clear();
    m_SlotOf.clear();
    m_RemovedSceneIds.clear();
    m_Peers.clear();
    m_SceneCount = static_cast<uint32_t>(sceneEntities.size());
    m_NextNetId = m_SceneCount + 1;
    for (uint32_t i = 0; i < m_SceneCount; ++i) {
        Entry& entry = m_Entries[AddEntry(sceneEntities[i], i + 1, true, false)];
        if (i < baseline.size()) entry.published = baseline[i]; // version 0: what clients loaded
        entry.lastPosition = entry.published.motion.Position();
    }
}

uint32_t ReplicationServer::AddEntry(EntityID entity, uint32_t netId, bool scene, bool player)
{
    uint32_t slot;
    if (!m_FreeSlots.empty()) {
        slot = m_FreeSlots.back();
        m_FreeSlots.pop_back();
    } else {
        slot = static_cast<uint32_t>(m_Entries.size());
        m_Entries.emplace_back();
    }
    Entry& e = m_Entries[slot];
    e = Entry();
    e.entity = entity;
    e.netId = netId;
    e.used = true;
    e.scene = scene;
    e.player = player;
    m_SlotOf[entity] = slot;
    return slot;
}

uint32_t ReplicationServer::RegisterPlayer(NetworkService& net, EntityID entity, uint32_t tick)
{
    const auto it = m_SlotOf.find(entity);
    if (it != m_SlotOf.end()) return m_Entries[it->second].netId;
    Entry& e = m_Entries[AddEntry(entity, m_NextNetId++, false, true)];
    e.published = CaptureNetState(*m_Registry, entity, false);
    e.publishedTick = e.lastTick = tick;
    e.lastPosition = e.published.motion.Position();

    Out packet;
    packet.Header(PacketType::NetSpawn, tick);
    packet.Value(m_Epoch);
    Write(packet, DescribeSpawn(e, tick));
    const uint32_t slot = m_SlotOf[entity];
    for (auto& [peer, p] : m_Peers) {
        if (p.entities.size() < m_Entries.size()) p.entities.resize(m_Entries.size());
        p.entities[slot] = PeerEntity();
        p.entities[slot].netId = e.netId;
        SendReliable(net, peer, packet);
    }
    return e.netId;
}

// A player's prefab children are created by each client together with the player
bool ReplicationServer::IsPartOfPlayer(EntityID entity) const
{
    for (int depth = 0; depth < 16; ++depth) {
        if (!m_Registry->HasComponent<RelationshipComponent>(entity)) return false;
        entity = m_Registry->GetComponent<RelationshipComponent>(entity).parent;
        if (entity == 0) return false;
        const auto it = m_SlotOf.find(entity);
        if (it != m_SlotOf.end() && m_Entries[it->second].player) return true;
    }
    return false;
}

NetSpawn ReplicationServer::DescribeSpawn(const Entry& entry, uint32_t tick) const
{
    NetSpawn s;
    s.netId = entry.netId;
    s.kind = entry.player ? NetSpawn::kPlayer : NetSpawn::kGeneric;
    s.category = static_cast<uint8_t>(m_Registry->GetCategory(entry.entity));
    s.name = std::string(m_Registry->GetName(entry.entity)).substr(0, kMaxString);
    s.state = entry.published;
    s.state.motion.SetPosition(entry.published.motion.Position()
        + entry.published.motion.Velocity() * (static_cast<float>(tick - entry.publishedTick) / 60.0f));
    if (m_Registry->HasComponent<ColliderComponent>(entry.entity)) {
        const auto& c = m_Registry->GetComponent<ColliderComponent>(entry.entity);
        s.autoBounds = c.GetAutoBounds();
        s.colliderOffset = c.GetLocalOffset();
        s.colliderSize = c.GetLocalSize();
        s.layerMask = c.GetLayerMask();
    }
    return s;
}

void ReplicationServer::AddPeer(NetworkService& net, ENetPeer* peer, EntityID own, uint32_t tick)
{
    Peer& p = m_Peers[peer];
    p = Peer();
    p.own = own;
    p.sent.resize(kSentHistory);
    p.entities.resize(m_Entries.size());

    for (Entry& e : m_Entries)
        if (e.used && m_Registry->IsAlive(e.entity)) Sample(e, tick);

    Out packet;
    const auto begin = [&](PacketType type) {
        packet.Clear();
        packet.Header(type, tick);
        packet.Value(m_Epoch);
    };

    begin(PacketType::NetDestroy);
    const size_t empty = packet.Size();
    for (const uint32_t id : m_RemovedSceneIds) {
        packet.Unsigned(id);
        if (packet.Size() > kMaxPacket - 8) { SendReliable(net, peer, packet); begin(PacketType::NetDestroy); }
    }
    if (packet.Size() > empty) SendReliable(net, peer, packet);

    begin(PacketType::NetSpawn);
    for (uint32_t slot = 0; slot < m_Entries.size(); ++slot) {
        const Entry& e = m_Entries[slot];
        if (!e.used) continue;
        PeerEntity& pe = p.entities[slot];
        pe.netId = e.netId;
        if (e.scene) continue;
        // The spawn carries the current state, and it always arrives
        pe.ackedMotion = pe.sentMotion = e.motionVersion;
        pe.ackedLook = pe.sentLook = e.lookVersion;
        if (e.entity == own || !m_Registry->IsAlive(e.entity)) continue;
        Write(packet, DescribeSpawn(e, tick));
        if (packet.Size() > kMaxPacket - 400) { SendReliable(net, peer, packet); begin(PacketType::NetSpawn); }
    }
    if (packet.Size() > empty) SendReliable(net, peer, packet);
}

void ReplicationServer::OnAck(ENetPeer* peer, uint16_t epoch, uint16_t seq, uint64_t bits)
{
    const auto it = m_Peers.find(peer);
    if (it == m_Peers.end() || epoch != m_Epoch || seq == 0) return;
    Peer& p = it->second;
    for (int i = -1; i < 64; ++i) {
        if (i >= 0 && !((bits >> i) & 1)) continue;
        const uint16_t acked = static_cast<uint16_t>(seq - 1 - i);
        SentPacket& packet = p.sent[acked % kSentHistory];
        if (!packet.pending || packet.seq != acked) continue;
        packet.pending = false;
        for (const SentRecord& r : packet.records) {
            PeerEntity& pe = p.entities[r.slot];
            if (pe.netId != r.netId) continue; // the slot now holds another entity
            pe.ackedMotion = std::max(pe.ackedMotion, r.motion);
            pe.ackedLook = std::max(pe.ackedLook, r.look);
        }
    }
}

// Publishes a new version if clients can't work out the current state by themselves.
void ReplicationServer::Sample(Entry& e, uint32_t tick)
{
    NetState now = CaptureNetState(*m_Registry, e.entity, e.scene);
    const glm::vec2 position = now.motion.Position();

    const uint32_t elapsed = tick - e.lastTick;
    if (!(now.look.flags & NetLook::kBody) && elapsed > 0) {
        // Moved by a script: derive the velocity
        const glm::vec2 moved = position - e.lastPosition;
        const bool teleported = std::abs(moved.x) > 2.0f || std::abs(moved.y) > 2.0f;
        const bool still = moved.x == 0.0f && moved.y == 0.0f;
        e.stillSamples = still ? e.stillSamples + 1 : 0;
        if (e.lastTick == 0 || teleported || e.stillSamples >= 2) {
            e.derivedVelocity = glm::vec2(0.0f);
        } else {
            // Scripts run once per frame, not per tick, so single samples are uneven
            const glm::vec2 v = moved * (60.0f / static_cast<float>(elapsed));
            e.derivedVelocity = e.derivedVelocity == glm::vec2(0.0f) ? v : e.derivedVelocity + (v - e.derivedVelocity) * 0.35f;
        }
        now.motion.SetVelocity(e.derivedVelocity);
    } else if (elapsed > 0) {
        // Gravity keeps adding a little speed to bodies resting on something
        const glm::vec2 moved = position - e.lastPosition;
        glm::vec2 v = now.motion.Velocity();
        if (std::abs(moved.x) < 0.002f && std::abs(v.x) < 0.5f) v.x = 0.0f;
        if (std::abs(moved.y) < 0.002f && std::abs(v.y) < 0.5f) v.y = 0.0f;
        now.motion.SetVelocity(v);
    }
    e.lastPosition = position;
    e.lastTick = tick;

    const NetMotion& old = e.published.motion;
    const glm::vec2 expected = old.Position() + old.Velocity() * (static_cast<float>(tick - e.publishedTick) / 60.0f);
    const glm::vec2 error = position - expected;
    const glm::vec2 velocityChange = now.motion.Velocity() - old.Velocity();
    const bool moving = old.vx || old.vy || now.motion.vx || now.motion.vy;
    const bool stopped = moving && !now.motion.vx && !now.motion.vy;
    const float tolerance = moving ? 0.03f : 0.004f;
    // Derived velocities are noisy, so they only count when they change clearly
    const float velocityTolerance = (now.look.flags & NetLook::kBody) ? 0.05f : 0.5f;
    if (std::abs(error.x) > tolerance || std::abs(error.y) > tolerance || now.motion.rotation != old.rotation
        || std::abs(velocityChange.x) > velocityTolerance || std::abs(velocityChange.y) > velocityTolerance || stopped) {
        e.published.motion = now.motion;
        e.publishedTick = tick;
        ++e.motionVersion;
    }
    if (!(now.look == e.published.look)) {
        e.published.look = now.look;
        ++e.lookVersion;
    }
}

void ReplicationServer::UpdateEntities(NetworkService& net, uint32_t tick)
{
    Out removed, spawned;
    removed.Header(PacketType::NetDestroy, tick);
    removed.Value(m_Epoch);
    const size_t empty = removed.Size();
    const auto sendAll = [&](Out& packet) {
        for (auto& [peer, p] : m_Peers) SendReliable(net, peer, packet);
    };

    for (uint32_t slot = 0; slot < m_Entries.size(); ++slot) {
        Entry& e = m_Entries[slot];
        if (!e.used) continue;
        if (m_Registry->IsAlive(e.entity)) {
            Sample(e, tick);
            continue;
        }
        removed.Unsigned(e.netId);
        if (e.scene) m_RemovedSceneIds.push_back(e.netId);
        m_SlotOf.erase(e.entity);
        e.used = false;
        m_FreeSlots.push_back(slot);
        if (removed.Size() > kMaxPacket - 8) {
            sendAll(removed);
            removed.Clear();
            removed.Header(PacketType::NetDestroy, tick);
            removed.Value(m_Epoch);
        }
    }
    if (removed.Size() > empty) sendAll(removed);

    // Anything else alive was created by a script or the game
    const auto beginSpawn = [&] {
        spawned.Clear();
        spawned.Header(PacketType::NetSpawn, tick);
        spawned.Value(m_Epoch);
    };
    beginSpawn();
    m_Registry->ForEachAlive([&](EntityID entity) {
        if (m_SlotOf.count(entity) || !m_Registry->HasComponent<TransformComponent>(entity) || IsPartOfPlayer(entity)) return;
        const uint32_t slot = AddEntry(entity, m_NextNetId++, false, false);
        Entry& e = m_Entries[slot];
        e.published = CaptureNetState(*m_Registry, entity, false);
        e.publishedTick = e.lastTick = tick;
        e.lastPosition = e.published.motion.Position();
        for (auto& [peer, p] : m_Peers) {
            if (p.entities.size() < m_Entries.size()) p.entities.resize(m_Entries.size());
            p.entities[slot] = PeerEntity();
            p.entities[slot].netId = e.netId;
        }
        Write(spawned, DescribeSpawn(e, tick));
        if (spawned.Size() > kMaxPacket - 400) { sendAll(spawned); beginSpawn(); }
    });
    if (spawned.Size() > empty) sendAll(spawned);
}

void ReplicationServer::Tick(NetworkService& net, uint32_t tick, const std::map<ENetPeer*, glm::vec2>& ownPositions)
{
    if (!m_Registry || m_Peers.empty()) return;
    const bool entities = tick % kSendInterval == 0;
    const bool own = tick % kOwnInterval == 0;
    if (entities) UpdateEntities(net, tick);
    if (!entities && !own) return;

    for (auto& [peer, p] : m_Peers) {
        const auto position = ownPositions.find(peer);
        SendSnapshots(net, peer, p, tick, own && position != ownPositions.end() ? &position->second : nullptr, entities);
    }
}

void ReplicationServer::SendSnapshots(NetworkService& net, ENetPeer* peer, Peer& p, uint32_t tick,
                                      const glm::vec2* own, bool entities)
{
    if (p.entities.size() < m_Entries.size()) p.entities.resize(m_Entries.size());
    // Wait for an acknowledgement about as long as one normally takes
    const uint32_t resendTicks = std::clamp<uint32_t>(
        static_cast<uint32_t>((peer->roundTripTime + 2 * peer->roundTripTimeVariance) / 16.67f) + 6, 8, 60);

    Out packet, record;
    SentPacket* sent = nullptr;
    int packets = 0;
    const auto begin = [&] {
        const uint16_t seq = p.nextSeq;
        p.nextSeq = p.nextSeq == 0xFFFF ? 1 : static_cast<uint16_t>(p.nextSeq + 1); // 0 means "none"
        sent = &p.sent[seq % kSentHistory];
        sent->seq = seq;
        sent->pending = true;
        sent->records.clear();
        packet.Clear();
        packet.Header(PacketType::NetSnapshot, tick);
        packet.Value(m_Epoch);
        packet.Value(seq);
        packet.Value(static_cast<uint8_t>(own ? 1 : 0));
        if (own) {
            packet.Value(own->x);
            packet.Value(own->y);
            own = nullptr;
        }
    };
    const auto flush = [&] {
        // Not channel 0: a lost spawn or welcome must not hold up movement until it is resent
        net.SendPacket(peer, kSnapshotChannel, packet.Bytes().data(), packet.Size(), 0);
        sent = nullptr;
        ++packets;
    };

    if (own) begin();
    const size_t count = entities ? m_Entries.size() : 0;
    size_t i = 0;
    for (; i < count; ++i) {
        const size_t slot = (p.cursor + i) % count;
        const Entry& e = m_Entries[slot];
        if (!e.used) continue;
        PeerEntity& pe = p.entities[slot];
        if (pe.netId != e.netId) continue; // not spawned for this peer
        // The player predicts their own movement; see own position above
        const bool isOwn = e.entity == p.own;
        const bool motion = !isOwn && pe.ackedMotion != e.motionVersion;
        const bool look = pe.ackedLook != e.lookVersion;
        if (!motion && !look) continue;
        const bool fresh = (motion && pe.sentMotion != e.motionVersion) || (look && pe.sentLook != e.lookVersion);
        if (!fresh && tick - pe.sentTick < resendTicks) continue;

        record.Clear();
        record.Unsigned(e.netId);
        record.Value(static_cast<uint8_t>((motion ? 1 : 0) | (look ? 2 : 0)));
        if (motion) {
            // Resends describe where the entity is now, not where it was
            NetMotion m = e.published.motion;
            if (e.publishedTick != tick) m.SetPosition(m.Position() + m.Velocity() * (static_cast<float>(tick - e.publishedTick) / 60.0f));
            Write(record, m);
        }
        if (look) Write(record, e.published.look);

        if (sent && packet.Size() + record.Size() > kMaxPacket) {
            flush();
            if (packets >= kMaxPacketsPerSend) break;
        }
        if (!sent) begin();
        packet.Append(record);
        if (motion) pe.sentMotion = e.motionVersion;
        if (look) pe.sentLook = e.lookVersion;
        pe.sentTick = tick;
        sent->records.push_back({static_cast<uint32_t>(slot), e.netId, motion ? e.motionVersion : 0, look ? e.lookVersion : 0});
    }
    if (sent) flush();
    // If it didn't all fit, continue from here next time
    if (count > 0) p.cursor = i < count ? (p.cursor + i) % count : 0;
}

// ============================================================
// Client
// ============================================================
void ReplicationClient::Reset(uint16_t epoch)
{
    m_Epoch = epoch;
    m_AckSeq = 0;
    m_AckBits = 0;
}

void ReplicationClient::Acknowledge(uint16_t seq)
{
    const int16_t ahead = static_cast<int16_t>(seq - m_AckSeq);
    if (m_AckSeq == 0) {
        m_AckSeq = seq;
    } else if (ahead > 0) {
        m_AckBits = ahead >= 64 ? 0 : m_AckBits << ahead;
        if (ahead <= 64) m_AckBits |= uint64_t(1) << (ahead - 1); // the previous newest
        m_AckSeq = seq;
    } else if (ahead < 0 && -ahead <= 64) {
        m_AckBits |= uint64_t(1) << (-ahead - 1);
    }
}

bool ReplicationClient::OnPacket(const void* data, size_t size, Sink& sink)
{
    if (size < sizeof(PacketHeader)) return false;
    PacketHeader header;
    std::memcpy(&header, data, sizeof(header));
    if (header.type != PacketType::NetSnapshot && header.type != PacketType::NetSpawn && header.type != PacketType::NetDestroy)
        return false;

    In in(static_cast<const uint8_t*>(data) + sizeof(header), size - sizeof(header));
    if (in.Value<uint16_t>() != m_Epoch || !in.Ok()) return true;

    // Parse everything before applying anything, so a malformed packet has no effect
    if (header.type == PacketType::NetSpawn) {
        std::vector<NetSpawn> spawns;
        while (in.Ok() && !in.Empty()) spawns.push_back(ReadSpawn(in));
        if (!in.Ok()) return true;
        for (const NetSpawn& s : spawns)
            if (!sink.OnNetKnows(s.netId)) sink.OnNetSpawn(s, header.tick);
    } else if (header.type == PacketType::NetDestroy) {
        std::vector<uint32_t> ids;
        while (in.Ok() && !in.Empty()) ids.push_back(in.Unsigned());
        if (!in.Ok()) return true;
        for (const uint32_t id : ids)
            if (sink.OnNetKnows(id)) sink.OnNetDestroy(id);
    } else {
        struct Record { uint32_t id; uint8_t mask; NetMotion motion; NetLook look; };
        std::vector<Record> records;
        const auto seq = in.Value<uint16_t>();
        const auto flags = in.Value<uint8_t>();
        glm::vec2 own(0.0f);
        if (flags > 1) in.Fail();
        if (flags & 1) { own.x = in.Float(); own.y = in.Float(); }
        while (in.Ok() && !in.Empty()) {
            Record r;
            r.id = in.Unsigned();
            r.mask = in.Value<uint8_t>();
            if (r.mask == 0 || r.mask > 3) in.Fail();
            if (r.mask & 1) r.motion = ReadMotion(in);
            if (r.mask & 2) r.look = ReadLook(in);
            records.push_back(std::move(r));
        }
        if (!in.Ok() || seq == 0) return true;

        bool complete = true;
        for (const Record& r : records) {
            if (!sink.OnNetKnows(r.id)) { complete = false; continue; }
            if (r.mask & 2) sink.OnNetLook(r.id, r.look);
            if (r.mask & 1) sink.OnNetMotion(r.id, r.motion, header.tick);
        }
        if (flags & 1) sink.OnNetOwnPosition(own, header.tick);
        if (complete) Acknowledge(seq); // otherwise the server sends it again
    }
    return true;
}
