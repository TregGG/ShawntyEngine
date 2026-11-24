// Network tests. Build and run with: make BUILD=asan nettest
// Under loss: unshare -rn sh -c 'ip link set lo up; tc qdisc add dev lo root netem delay 30ms 10ms loss 15%; ./asan/nettest'
#include "../assets/assetmanager.h"
#include "../core/endpoint.h"
#include "../core/projectpaths.h"
#include "../levels/datadrivenscene.h"
#include "../services/clientclock.h"
#include "../services/contentsync.h"
#include "../services/networkservice.h"
#include "../services/replication.h"
#include "../test/testnetworkcontrol.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <thread>

static int s_Failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++s_Failures; std::printf("FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static std::mt19937 s_Rng(12345);
static int Rand(int n) { return static_cast<int>(s_Rng() % static_cast<unsigned>(n)); }
static float RandF(float range) { return (static_cast<float>(Rand(20001)) / 10000.0f - 1.0f) * range; }
static void Sleep(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

// What a client believes
struct Mirror : ReplicationClient::Sink
{
    struct Known { NetMotion motion; NetLook look; uint32_t tick = 0; std::string name; };
    std::map<uint32_t, Known> entities;
    std::set<uint32_t> destroyed;

    bool OnNetKnows(uint32_t id) override { return entities.count(id) > 0; }
    void OnNetSpawn(const NetSpawn& s, uint32_t tick) override
    {
        CHECK(!destroyed.count(s.netId), "network ID %u was reused", s.netId);
        entities[s.netId] = {s.state.motion, s.state.look, tick, s.name};
    }
    void OnNetDestroy(uint32_t id) override { entities.erase(id); destroyed.insert(id); }
    void OnNetMotion(uint32_t id, const NetMotion& m, uint32_t tick) override { entities[id].motion = m; entities[id].tick = tick; }
    void OnNetLook(uint32_t id, const NetLook& l) override { entities[id].look = l; }
    void OnNetOwnPosition(const glm::vec2&, uint32_t) override {}
};

struct Client
{
    NetworkService net;
    ReplicationClient replication;
    Mirror mirror;
    bool connected = false;
    size_t largest = 0, bytes = 0;
};

// Random changes over a real connection; afterwards every client must agree with the server.
static void TestConvergence(int port, int entityCount, int clientCount)
{
    EntityRegistryService registry;
    registry.Init();
    std::vector<EntityID> scene;
    std::vector<NetState> baseline;
    for (int i = 0; i < entityCount; ++i) {
        const EntityID e = registry.Create(EntityCategory::Environment, "S" + std::to_string(i), "Test");
        TransformComponent t;
        t.position = {RandF(50), RandF(50)};
        registry.AddComponent<TransformComponent>(e, t);
        SpriteComponent2D sprite;
        sprite.objectId = "box";
        registry.AddComponent<SpriteComponent2D>(e, sprite);
        scene.push_back(e);
        baseline.push_back(CaptureNetState(registry, e, true));
    }

    NetworkService net;
    net.Init();
    CHECK(net.Host(port), "host");
    ReplicationServer server;
    server.BeginScene(registry, scene, baseline);

    std::vector<ENetPeer*> peers;
    net.OnClientConnected = [&](ENetPeer* p) { peers.push_back(p); };
    net.SetPacketCallback([&](ENetPeer* p, void* data, size_t size) {
        if (size < sizeof(ClientInputPacket)) return;
        ClientInputPacket input;
        std::memcpy(&input, data, sizeof(input));
        server.OnAck(p, input.epoch, input.ackSeq, input.ackBits);
    });

    std::vector<std::unique_ptr<Client>> clients;
    uint32_t tick = 1;
    const auto addClient = [&] {
        auto c = std::make_unique<Client>();
        Client* raw = c.get();
        raw->net.Init();
        raw->replication.Reset(server.GetEpoch());
        // Loaded from the scene file
        for (int i = 0; i < entityCount; ++i) raw->mirror.entities[static_cast<uint32_t>(i) + 1] = {baseline[i].motion, baseline[i].look, 0, "S"};
        raw->net.OnConnected = [raw] { raw->connected = true; };
        raw->net.SetPacketCallback([raw](ENetPeer*, void* data, size_t size) {
            raw->largest = std::max(raw->largest, size);
            raw->bytes += size;
            raw->replication.OnPacket(data, size, raw->mirror);
        });
        raw->net.Connect("127.0.0.1", port);
        const size_t expected = peers.size() + 1;
        for (int i = 0; i < 3000 && (peers.size() < expected || !raw->connected); ++i) {
            net.Update(0.0f);
            for (auto& other : clients) other->net.Update(0.0f);
            raw->net.Update(0.0f);
            Sleep(10);
        }
        CHECK(raw->connected, "client did not connect");
        clients.push_back(std::move(c));
        server.AddPeer(net, peers.back(), 0, tick);
    };
    const auto step = [&] {
        ++tick;
        net.Update(0.0f);
        server.Tick(net, tick, {});
        registry.Update(0.016f);
        net.Flush();
        for (auto& c : clients) {
            c->net.Update(0.0f);
            ClientInputPacket input{};
            input.header.type = PacketType::ClientInput;
            input.epoch = c->replication.GetEpoch();
            input.ackSeq = c->replication.GetAckSeq();
            input.ackBits = c->replication.GetAckBits();
            c->net.SendPacket(c->net.GetServerPeer(), 0, &input, sizeof(input), 0);
            c->net.Flush();
        }
        Sleep(3);
    };

    addClient();
    std::vector<EntityID> alive = scene;
    for (int round = 0; round < 900; ++round) {
        if (round == 300 || round == 600) for (int i = 1; i < clientCount; ++i) if (clients.size() < static_cast<size_t>(clientCount)) { addClient(); break; }
        for (int i = 0; i < 8 && !alive.empty(); ++i) {
            const EntityID e = alive[Rand(static_cast<int>(alive.size()))];
            auto& t = registry.GetComponent<TransformComponent>(e);
            switch (Rand(5)) {
                case 0: t.position += glm::vec2(RandF(1), RandF(1)); break;
                case 1: t.rotation = RandF(10); break;
                case 2: t.size = {RandF(5), RandF(5)}; break;
                case 3: registry.GetComponent<SpriteComponent2D>(e).frameIndex = Rand(9); break;
                case 4: t.position = {RandF(500), RandF(500)}; break;
            }
        }
        if (Rand(4) == 0) {
            const EntityID e = registry.Create(EntityCategory::Projectile, "B", "Test");
            TransformComponent t;
            t.position = {RandF(50), RandF(50)};
            registry.AddComponent<TransformComponent>(e, t);
            SpriteComponent2D sprite;
            sprite.objectId = "bullet";
            registry.AddComponent<SpriteComponent2D>(e, sprite);
            registry.AddComponent<ColliderComponent>(e, ColliderComponent());
            RigidBodyComponent rb;
            rb.SetType(BodyType::Dynamic);
            registry.AddComponent<RigidBodyComponent>(e, rb);
            alive.push_back(e);
        }
        if (Rand(5) == 0 && alive.size() > 2) {
            const size_t i = Rand(static_cast<int>(alive.size()));
            registry.Destroy(alive[i]);
            registry.Destroy(alive[i]); // twice must be harmless
            alive.erase(alive.begin() + static_cast<long>(i));
        }
        step();
    }
    const auto countWrong = [&] {
        size_t wrong = 0;
        for (auto& c : clients) wrong += c->mirror.entities.size() > alive.size() ? c->mirror.entities.size() - alive.size() : 0;
        for (const EntityID e : alive) {
            const NetState want = CaptureNetState(registry, e, false);
            for (auto& c : clients) {
                bool found = false;
                for (auto& [id, k] : c->mirror.entities) {
                    const glm::vec2 d = k.motion.Position() - want.motion.Position();
                    if (std::abs(d.x) < 0.04f && std::abs(d.y) < 0.04f && k.motion.vx == 0 && k.motion.vy == 0
                        && k.motion.rotation == want.motion.rotation && k.look == want.look) { found = true; break; }
                }
                if (!found) ++wrong;
            }
        }
        return wrong;
    };

    // Changes have stopped: every client must catch up
    const auto settleStart = std::chrono::steady_clock::now();
    const auto settled = [&] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - settleStart).count(); };
    size_t wrong = 1;
    while (wrong > 0 && settled() < 45) {
        for (int i = 0; i < 30; ++i) step();
        wrong = countWrong();
    }
    const double settleTime = settled();
    for (int i = 0; i < 1500; ++i) step(); // last acknowledgements

    // Quiet now: nothing but acknowledgements
    for (auto& c : clients) c->bytes = 0;
    for (int i = 0; i < 150; ++i) step();
    for (size_t i = 0; i < clients.size(); ++i) {
        Client& c = *clients[i];
        CHECK(c.bytes == 0, "client %zu still received %zu bytes when idle", i, c.bytes);
        CHECK(c.largest <= 1200, "a packet of %zu bytes was sent", c.largest);
        CHECK(c.mirror.entities.size() == alive.size(), "client %zu has %zu entities, server %zu", i, c.mirror.entities.size(), alive.size());
    }
    wrong = countWrong();
    CHECK(wrong == 0, "%zu entities differ", wrong);
    std::printf("convergence: %zu entities, %zu clients, %zu wrong, caught up after %.1f s, rtt %u ms\n", alive.size(), clients.size(), wrong,
                settleTime, peers[0]->roundTripTime);
    for (ENetPeer* p : peers) CHECK(p->state == ENET_PEER_STATE_CONNECTED, "a client lost its connection");
    for (auto& c : clients) c->net.Shutdown();
    net.Shutdown();
}

// Extrapolating from what was sent must stay close to the truth, with few packets.
static void TestDeadReckoning(int port)
{
    EntityRegistryService registry;
    registry.Init();
    const EntityID e = registry.Create(EntityCategory::Environment, "Platform", "Test");
    registry.AddComponent<TransformComponent>(e);
    NetworkService net;
    net.Init();
    net.Host(port);
    ReplicationServer server;
    server.BeginScene(registry, {e}, {CaptureNetState(registry, e, true)});
    ENetPeer* peer = nullptr;
    net.OnClientConnected = [&](ENetPeer* p) { peer = p; };
    Client c;
    c.net.Init();
    c.replication.Reset(server.GetEpoch());
    c.mirror.entities[1] = {};
    c.net.SetPacketCallback([&](ENetPeer*, void* d, size_t n) { c.bytes += n; c.replication.OnPacket(d, n, c.mirror); });
    c.net.Connect("127.0.0.1", port);
    for (int i = 0; i < 3000 && (!peer || !c.net.IsConnected()); ++i) { net.Update(0); c.net.Update(0); Sleep(5); }
    if (!peer) { CHECK(false, "connect"); return; }
    uint32_t tick = 1;
    server.AddPeer(net, peer, 0, tick);
    net.SetPacketCallback([&](ENetPeer* p, void* d, size_t n) {
        ClientInputPacket in;
        if (n < sizeof(in)) return;
        std::memcpy(&in, d, sizeof(in));
        server.OnAck(p, in.epoch, in.ackSeq, in.ackBits);
    });
    float worst = 0;
    for (int i = 0; i < 1200; ++i) {
        ++tick;
        // A script moving a platform back and forth, 3 units per second
        const float phase = std::fmod(static_cast<float>(tick) / 60.0f, 4.0f);
        registry.GetComponent<TransformComponent>(e).position.x = phase < 2 ? phase * 3 : (4 - phase) * 3;
        server.Tick(net, tick, {});
        net.Flush();
        c.net.Update(0);
        ClientInputPacket in{};
        in.header.type = PacketType::ClientInput;
        in.epoch = c.replication.GetEpoch(); in.ackSeq = c.replication.GetAckSeq(); in.ackBits = c.replication.GetAckBits();
        c.net.SendPacket(c.net.GetServerPeer(), 0, &in, sizeof(in), 0);
        c.net.Flush();
        Sleep(1);
        c.net.Update(0);
        net.Update(0);
        const auto& k = c.mirror.entities[1];
        const float x = k.motion.Position().x + k.motion.Velocity().x * static_cast<float>(tick - k.tick) / 60.0f;
        if (i > 10) worst = std::max(worst, std::abs(x - registry.GetComponent<TransformComponent>(e).position.x));
    }
    std::printf("dead reckoning: worst error %.3f units, %zu bytes in 20 s\n", worst, c.bytes);
    if (peer->roundTripTime < 20) {
        CHECK(worst < 0.4f, "error %.3f", worst);
        CHECK(c.bytes < 20000, "%zu bytes", c.bytes);
    }
    c.net.Shutdown();
    net.Shutdown();
}

static std::vector<uint8_t> RandomPacket(const std::vector<std::vector<uint8_t>>& seeds)
{
    std::vector<uint8_t> p;
    if (!seeds.empty() && Rand(4) != 0) {
        p = seeds[Rand(static_cast<int>(seeds.size()))];
        for (int i = Rand(6); i > 0 && !p.empty(); --i) {
            switch (Rand(4)) {
                case 0: p[Rand(static_cast<int>(p.size()))] = static_cast<uint8_t>(s_Rng()); break;
                case 1: p.resize(Rand(static_cast<int>(p.size()) + 1)); break;
                case 2: p.push_back(static_cast<uint8_t>(s_Rng())); break;
                case 3: p[Rand(static_cast<int>(p.size()))] = Rand(2) ? 0xFF : 0x80; break;
            }
        }
    } else {
        p.resize(Rand(200));
        for (auto& b : p) b = static_cast<uint8_t>(s_Rng());
        if (!p.empty()) p[0] = static_cast<uint8_t>(Rand(20));
    }
    return p;
}

// Feeds garbage to everything that reads packets. The sanitizers do the checking.
static void TestFuzz(int port, int rounds)
{
    const std::string root = std::filesystem::temp_directory_path() / ("nettest-" + std::to_string(getpid()));
    std::filesystem::create_directories(root + "/scenes");
    std::ofstream(root + "/scenes/main.scene") << R"({"version":1,"scene":{"entities":[
        {"editorId":"a","name":"A","components":{"transform":{"position":[0,0]},"sprite":{"layer":"Foreground"},"animator":{},"collider":{"autoBounds":true},"rigidbody":{"bodyType":"Dynamic"}}},
        {"editorId":"b","name":"B","components":{"transform":{"position":[1,0]}}},
        {"editorId":"c","name":"C","components":{"transform":{"position":[5,0]}}}],"relationships":[{"parent":"a","children":["b"]}]}})";
    ProjectPaths::SetRoot(root);
    AssetManager assets;
    assets.Initialize(root, true);

    std::vector<std::vector<uint8_t>> toServer, toClient;
    NetworkService serverNet, clientNet;
    serverNet.Init();
    clientNet.Init();
    ContentServer contentServer;
    std::string error;
    CHECK(contentServer.Load(root, error), "%s", error.c_str());
    ContentClient contentClient(root + "/cache");
    TestNetworkControl serverControl(&assets), clientControl(&assets);
    DataDrivenScene serverScene(&assets, root + "/scenes/main.scene"), clientScene(&assets, root + "/scenes/main.scene");
    clientScene.SetScriptsEnabled(false);
    serverScene.SetScriptsEnabled(false);
    serverScene.OnEnter();
    clientScene.OnEnter();
    serverControl.SetAssets(&assets);
    clientControl.SetAssets(&assets);
    serverControl.SetNetworkService(&serverNet);
    clientControl.SetNetworkService(&clientNet);
    serverControl.SetContentServer(&contentServer);
    clientControl.SetContentClient(&contentClient);
    serverControl.BindScene(&serverScene, nullptr);
    clientControl.BindScene(&clientScene, nullptr);
    clientNet.OnConnected = [&] { contentClient.OnConnected(clientNet); };
    int downloads = 0;
    contentClient.OnContent = [&](const std::string&) { ++downloads; contentClient.SendReady(clientNet); };
    clientControl.OnClientConnectedCallback = [&](const std::string&) {
        clientScene.OnExit();
        clientControl.OnSceneChanged();
        clientScene.OnEnter();
        clientControl.SpawnLocalPlayer();
    };

    ENetPeer* peer = nullptr;
    bool record = true;
    serverNet.SetPacketCallback([&](ENetPeer* p, void* d, size_t n) {
        peer = p;
        if (record) toServer.emplace_back(static_cast<uint8_t*>(d), static_cast<uint8_t*>(d) + n);
        serverControl.OnPacketReceived(p, d, n);
    });
    clientNet.SetPacketCallback([&](ENetPeer* p, void* d, size_t n) {
        if (record) toClient.emplace_back(static_cast<uint8_t*>(d), static_cast<uint8_t*>(d) + n);
        clientControl.OnPacketReceived(p, d, n);
    });
    const auto run = [&](int ticks) {
        for (int i = 0; i < ticks; ++i) {
            serverNet.Update(0); clientNet.Update(0);
            serverControl.Tick(1 / 60.0f); clientControl.Tick(1 / 60.0f);
            clientControl.Update(1 / 60.0f);
            serverScene.Update(1 / 60.0f); clientScene.Update(1 / 60.0f);
            serverNet.Flush(); clientNet.Flush();
            Sleep(1);
        }
    };
    serverNet.Host(port);
    clientNet.Connect("127.0.0.1", port);
    for (int i = 0; i < 300 && clientControl.GetLocalPlayerID() == 0; ++i) run(50);
    CHECK(downloads == 1 && clientControl.GetLocalPlayerID() != 0, "the real client and server did not connect");
    run(60);

    // The real client's scene follows the real server's
    {
        auto& sr = serverScene.registry;
        auto& cr = clientScene.registry;
        const EntityID sa = serverScene.GetSceneEntities()[0], sb = serverScene.GetSceneEntities()[1], sc = serverScene.GetSceneEntities()[2];
        const EntityID ca = clientScene.GetSceneEntities()[0], cb = clientScene.GetSceneEntities()[1], cc = clientScene.GetSceneEntities()[2];
        sr.GetComponent<RigidBodyComponent>(sa).SetUseGravity(false);
        sr.GetComponent<RigidBodyComponent>(sa).SetVelocity({0, 0}); // the player may have bumped into it
        sr.GetComponent<TransformComponent>(sa).position = {30, 4};
        sr.GetComponent<TransformComponent>(sa).rotation = 1.0f;
        sr.GetComponent<TransformComponent>(sb).localPosition = {0, 3}; // child
        sr.Destroy(sc);
        const EntityID made = sr.Create(EntityCategory::Projectile, "Made", "Test");
        TransformComponent t;
        t.position = {-20, 5};
        t.size = {2, 3};
        sr.AddComponent<TransformComponent>(made, t);
        sr.AddComponent<SpriteComponent2D>(made, SpriteComponent2D());
        sr.AddComponent<ColliderComponent>(made, ColliderComponent({0, 0}, {1, 1}, true));
        run(400);
        const auto near = [](glm::vec2 a, glm::vec2 b) { return glm::length(a - b) < 0.1f; };
        CHECK(near(cr.GetComponent<TransformComponent>(ca).position, {30, 4}), "moved body at %.2f %.2f", cr.GetComponent<TransformComponent>(ca).position.x, cr.GetComponent<TransformComponent>(ca).position.y);
        CHECK(std::abs(cr.GetComponent<TransformComponent>(ca).rotation - 1.0f) < 0.01f, "rotation");
        CHECK(cr.GetComponent<RigidBodyComponent>(ca).GetType() == BodyType::Kinematic, "clients must not simulate server bodies");
        CHECK(near(cr.GetComponent<TransformComponent>(cb).GetWorldPosition(), {30, 7}), "child of a moved parent at %.2f %.2f", cr.GetComponent<TransformComponent>(cb).GetWorldPosition().x, cr.GetComponent<TransformComponent>(cb).GetWorldPosition().y);
        CHECK(!cr.IsAlive(cc), "destroyed scene entity still exists");
        const auto find = [&](const char* name) { EntityID f = 0; cr.ForEachAlive([&](EntityID e) { if (cr.GetName(e) == name) f = e; }); return f; };
        const EntityID cm = find("Made");
        CHECK(cm != 0, "spawned entity missing");
        if (cm) {
            CHECK(near(cr.GetComponent<TransformComponent>(cm).position, {-20, 5}) && near(cr.GetComponent<TransformComponent>(cm).size, {2, 3}), "spawned entity state");
            CHECK(cr.HasComponent<ColliderComponent>(cm) && cr.GetComponent<ColliderComponent>(cm).IsTrigger(), "spawned collider");
        }
        int players = 0;
        cr.ForEachAlive([&](EntityID e) { players += cr.GetCategory(e) == EntityCategory::Player; });
        CHECK(players == 1, "the client sees %d copies of itself", players);

        // A script-style platform keeps following smoothly
        float worst = 0;
        for (int i = 0; i < 300; ++i) {
            sr.GetComponent<TransformComponent>(made).position.x += 0.05f;
            run(1);
            if (cm && i > 60) {
                const float x = cr.GetComponent<TransformComponent>(cm).position.x - sr.GetComponent<TransformComponent>(made).position.x;
                worst = std::max(worst, std::abs(x));
            }
        }
        std::printf("real client follows a moving platform within %.2f units (it runs ahead by its lead)\n", worst);
        CHECK(worst < 1.0f, "platform error %.2f", worst);
        sr.Destroy(made);
        run(60);
        CHECK(find("Made") == 0, "destroyed entity still exists");
    }
    run(60);
    record = false;
    std::printf("fuzz: %zu client and %zu server packets as seeds\n", toServer.size(), toClient.size());

    for (int i = 0; i < rounds; ++i) {
        // Copies in exactly sized heap blocks, so reading past the end is caught
        auto a = RandomPacket(toServer);
        auto b = RandomPacket(toClient);
        if (peer && peer->state == ENET_PEER_STATE_CONNECTED) serverControl.OnPacketReceived(peer, a.data(), a.size());
        if (clientNet.GetMode() == NetworkMode::Client) clientControl.OnPacketReceived(clientNet.GetServerPeer(), b.data(), b.size());
        if (i % 200 == 0) {
            run(1);
            if (clientNet.GetMode() != NetworkMode::Client) { // thrown out for misbehaving: join again
                clientNet.Connect("127.0.0.1", port);
                for (int j = 0; j < 20 && !contentClient.IsReady(); ++j) run(20);
            }
        }
    }

    // Decoder alone, always in the right epoch so it gets past the first check
    Mirror mirror;
    ReplicationClient decoder;
    for (int i = 0; i < rounds * 4; ++i) {
        auto p = RandomPacket(toClient);
        if (p.size() >= 7) {
            p[0] = static_cast<uint8_t>(static_cast<int>(PacketType::NetSnapshot) + Rand(3));
            p[5] = 7; p[6] = 0;
        }
        decoder.Reset(7);
        mirror.entities.clear();
        mirror.destroyed.clear();
        for (uint32_t id = 1; id < 20; ++id) mirror.entities[id] = {};
        decoder.OnPacket(p.data(), p.size(), mirror);
    }
    for (int i = 0; i < rounds; ++i) {
        std::string s;
        for (int n = Rand(40); n > 0; --n) s += "ab1:/. -_\\\0\xff"[Rand(12)];
        std::string host, code;
        int parsedPort;
        ParseEndpoint(s, host, parsedPort, code);
        if (IsSafeContentPath(s)) CHECK(s.find("..") == std::string::npos, "unsafe path accepted: %s", s.c_str());
    }
    for (const char* bad : {"../x.scene", "scenes/../x.scene", "/etc/passwd", "scenes/a/b.scene", "scripts/a.py", "scenes/.scene", "scenes/x.py", "textures/x.png", "server.json", "manifest.json", ""})
        CHECK(!IsSafeContentPath(bad), "unsafe path accepted: %s", bad);

    clientNet.Shutdown();
    serverNet.Shutdown();
    ProjectPaths::SetRoot(ProjectPaths::kDefaultRoot);
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const int rounds = argc > 1 ? std::atoi(argv[1]) : 50000;
    const int port = 20000 + static_cast<int>(getpid() % 20000);
    TestConvergence(port, 60, 3);
    TestConvergence(port + 1, 400, 2);
    TestDeadReckoning(port + 2);
    TestFuzz(port + 3, rounds);
    std::printf(s_Failures ? "%d FAILED\n" : "all network tests passed\n", s_Failures);
    return s_Failures != 0;
}
