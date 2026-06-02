#include "testgame.h"

#include "../core/engine.h"
#include "../core/input.h"
#include "../assets/assetmanager.h"
#include "../core/enginedebug.h"
#include "../services/networkservice.h"
#include "testnetworkcontrol.h"
#include "../levels/datadrivenscene.h"

#include "../objects/ui/uipanel.h"
#include "../objects/ui/uitext.h"
#include "../objects/ui/uibutton.h"
#include "../objects/ui/uiinputfield.h"
#include "../services/base/eventservice.h"

#include <string>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <GLFW/glfw3.h>

NetworkControl* TestGame::CreateNetworkControl() {
    return new TestNetworkControl(&m_AssetManager);
}

namespace fs = std::filesystem;

// Helper: writes small compiled assets for testing
bool WriteTestCompiledAssets(const std::string& compiledRoot)
{
    try {
        fs::path root(compiledRoot);
        fs::create_directories(root / "textures");
        fs::create_directories(root / "spritesheets");
        fs::create_directories(root / "animations");
        fs::create_directories(root / "objects");

        // --- Write a tiny 2x2 RGBA TGA ---
        std::ofstream tex((root / "textures" / "test.tga").string(), std::ios::binary);
        if (!tex.is_open()) return false;

        unsigned char header[18] = {0};
        header[2] = 2; // uncompressed true-color image
        header[12] = 2; // width low
        header[13] = 0; // width high
        header[14] = 2; // height low
        header[15] = 0; // height high
        header[16] = 32; // bpp
        tex.write(reinterpret_cast<char*>(header), 18);

        // 4 pixels BGRA (2x2)
        unsigned char pixels[16] = {
            0, 0, 255, 255,   // red
            0, 255, 0, 255,   // green
            255, 0, 0, 255,   // blue
            255, 255, 255, 255 // white
        };
        tex.write(reinterpret_cast<char*>(pixels), sizeof(pixels));
        tex.close();

        // --- spritesheet ---
        std::ofstream ss((root / "spritesheets" / "test.ssheet").string());
        ss << "texture:test\n";
        ss << "0: x=0 y=0 w=2 h=2\n";
        ss.close();

        // --- animation set ---
        std::ofstream anim((root / "animations" / "test.anim").string());
        anim << "clip:idle\n";
        anim << "0: frame=0 duration=1.0\n";
        anim.close();

        // --- object descriptor ---
        std::ofstream obj((root / "objects" / "testobj.objasset").string());
        obj << "spritesheet:test\n";
        obj << "animations:test\n";
        obj.close();

        ENGINE_LOG("Wrote test compiled assets to %s", compiledRoot.c_str());
        return true;
    } catch (const std::exception& e) {
        ENGINE_ERROR("WriteTestCompiledAssets error: %s", e.what());
        return false;
    }
}

// ============================================================
// Create the host/join UI on the active scene's registry
// ============================================================
void TestGame::CreateNetworkUI(Scene* scene) {
    bool isServer = IsServer();
    bool isConnected = (m_NetService && m_NetService->IsConnected());

    if (isServer || isConnected) return;

    // UI Panel Background
    auto panel = std::make_unique<UIPanel>(scene, "MainPanel");
    panel->Position = glm::vec2(50.0f, 50.0f);
    panel->Size = glm::vec2(300.0f, 250.0f);
    panel->BackgroundColor = glm::vec4(0.2f, 0.2f, 0.2f, 0.8f);

    // UI Text
    auto text = std::make_unique<UIText>(scene, "TitleText", &m_FontEngine);
    text->Position = glm::vec2(20.0f, 10.0f);
    text->Text = "Multiplayer Test";
    text->TextColor = glm::vec3(1.0f, 0.8f, 0.0f); // Gold
    panel->AddChild(std::move(text));

    // IP Input Field
    auto inputF = std::make_unique<UIInputField>(scene, "IPInput", m_EventService, &m_FontEngine);
    inputF->Position = glm::vec2(20.0f, 60.0f);
    inputF->Size = glm::vec2(250.0f, 40.0f);

    UIText* rawInputText = inputF->GetTextElement();
    rawInputText->Size = inputF->Size;
    rawInputText->HorizontalAlign = TextAlignment::Left;
    rawInputText->VerticalAlign = VerticalAlignment::Middle;
    rawInputText->Position = glm::vec2(5.0f, 0.0f);
    rawInputText->Text = "127.0.0.1"; // Default IP

    // Host Button
    auto hostBtn = std::make_unique<UIButton>(scene, "HostButton", m_EventService);
    hostBtn->Position = glm::vec2(20.0f, 120.0f);
    hostBtn->Size = glm::vec2(120.0f, 40.0f);

    auto hostText = std::make_unique<UIText>(scene, "HostBtnText", &m_FontEngine);
    hostText->Position = glm::vec2(0.0f, 0.0f);
    hostText->Size = hostBtn->Size;
    hostText->HorizontalAlign = TextAlignment::Center;
    hostText->VerticalAlign = VerticalAlignment::Middle;
    hostText->Text = "Host";
    hostText->TextColor = glm::vec3(1.0f);
    hostBtn->AddChild(std::move(hostText));

    // Join Button
    auto joinBtn = std::make_unique<UIButton>(scene, "JoinButton", m_EventService);
    joinBtn->Position = glm::vec2(150.0f, 120.0f);
    joinBtn->Size = glm::vec2(120.0f, 40.0f);

    auto joinText = std::make_unique<UIText>(scene, "JoinBtnText", &m_FontEngine);
    joinText->Position = glm::vec2(0.0f, 0.0f);
    joinText->Size = joinBtn->Size;
    joinText->HorizontalAlign = TextAlignment::Center;
    joinText->VerticalAlign = VerticalAlignment::Middle;
    joinText->Text = "Join";
    joinText->TextColor = glm::vec3(1.0f);
    joinBtn->AddChild(std::move(joinText));

    // Status Text
    auto statusTxt = std::make_unique<UIText>(scene, "StatusText", &m_FontEngine);
    statusTxt->Position = glm::vec2(20.0f, 180.0f);
    statusTxt->Text = "Offline";
    statusTxt->TextColor = glm::vec3(0.5f, 0.5f, 0.5f);
    m_StatusText = statusTxt.get();
    panel->AddChild(std::move(statusTxt));

    // Callbacks
    hostBtn->OnClickCallback = [this]() {
        if (m_NetService) {
            ENGINE_LOG("Host button clicked. Launching dedicated server...");
#ifdef _WIN32
            std::system("start bin\\framework.exe --server");
#elif defined(__APPLE__)
            std::system("open -n ./bin/framework --args --server");
#else
            std::system("./bin/framework --server &");
#endif
            m_NetService->Connect("127.0.0.1", 7777);
            if (m_StatusText) m_StatusText->Text = "Connecting...";
        }
    };

    joinBtn->OnClickCallback = [this, inputPtr = inputF.get()]() {
        if (m_NetService) {
            std::string ip = inputPtr->GetTextElement()->Text;
            if (ip.empty()) ip = "127.0.0.1";
            ENGINE_LOG("Join button clicked. Connecting to %s:7777", ip.c_str());
            m_NetService->Connect(ip, 7777);
            if (m_StatusText) m_StatusText->Text = "Connecting...";
        }
    };

    panel->AddChild(std::move(inputF));
    panel->AddChild(std::move(hostBtn));
    panel->AddChild(std::move(joinBtn));

    scene->registry.AddUIElement(std::move(panel));
}

// ============================================================
// OnInit
// ============================================================
bool TestGame::OnInit()
{
    ENGINE_LOG("OnInit");

    const std::string compiledRoot = "test_compiled";
    if (!WriteTestCompiledAssets(compiledRoot))
    {
        ENGINE_ERROR("Failed to write compiled assets");
        return false;
    }

    bool isServer = IsServer();
    if (!m_AssetManager.Initialize(compiledRoot, isServer))
    {
        ENGINE_ERROR("AssetManager failed to initialize");
        return false;
    }

    if (!isServer) {
        ENGINE_LOG("AssetManager initialized, initializing RenderManager");
        if (!m_RenderManager.Initialize())
        {
            ENGINE_ERROR("RenderManager failed to initialize");
            return false;
        }

        ENGINE_LOG("RenderManager initialized, creating FontEngine");
        m_FontEngine.Init();
        m_FontEngine.LoadFont("assets/comic.ttf", 24);
    } else {
        ENGINE_LOG("Server mode: skipping RenderManager and FontEngine initialization");
    }

    // ---- Create DataDrivenScenes from JSON ----
    ENGINE_LOG("Creating DataDrivenScenes from JSON files");
    m_DDScene1 = new DataDrivenScene(&m_AssetManager, "test_compiled/scenes/testscene1.scene");
    m_DDScene2 = new DataDrivenScene(&m_AssetManager, "test_compiled/scenes/testscene2.scene");

    m_SceneManager.SetInitialScene(m_DDScene1);

    if (!isServer) {
        m_RenderManager.BindScene(m_DDScene1);
    }

    // ---- Wire networking (Option C: game code handles this) ----
    if (m_NetControl) {
        m_NetControl->BindScene(m_DDScene1, nullptr); // Input set later in OnInput
    }

    // Look up the trigger entity from the loaded scene
    m_TriggerID = m_DDScene1->GetEntityByEditorId("scene_trigger");
    if (m_TriggerID == 0) {
        ENGINE_WARN("Could not find 'scene_trigger' entity in scene");
    }

    // Create UI for host/join (on the scene's registry)
    // CreateNetworkUI(m_DDScene1); // Comment out hardcoded UI to prefer Data-Driven UI

    m_DDScene1->registry.SetUIActionCallback([this](const std::string& action, const std::string& target) {
        if (action == "Host") {
            ENGINE_LOG("Host Action triggered. Launching dedicated server...");
#ifdef _WIN32
            std::system("start bin\\framework.exe --server");
#elif defined(__APPLE__)
            std::system("open -n ./bin/framework --args --server");
#else
            std::system("./bin/framework --server &");
#endif
            if (m_NetService) m_NetService->Connect("127.0.0.1", 7777);
        } else if (action == "Join") {
            std::string ip = "127.0.0.1";
            if (!target.empty() && m_DDScene1) {
                if (auto inputF = m_DDScene1->registry.FindUIElementRecursive(target)) {
                    if (auto uiInput = dynamic_cast<UIInputField*>(inputF)) {
                        if (uiInput->GetTextElement()) ip = uiInput->GetTextElement()->Text;
                    }
                }
            }
            ENGINE_LOG("Join Action triggered. Connecting to %s:7777", ip.c_str());
            if (m_NetService) m_NetService->Connect(ip, 7777);
        } else if (action == "ToggleActive") {
            if (m_DDScene1) {
                EntityID eid = m_DDScene1->GetEntityByEditorId(target);
                if (eid != 0) {
                    ENGINE_LOG("ToggleActive triggered for %s (Not yet implemented in core components)", target.c_str());
                }
            }
        }
    });

    // ---- Server: scene transition when all players in trigger ----
    if (isServer && m_NetControl && m_TriggerID != 0) {
        ENGINE_LOG("Server mode: trigger-based scene transition enabled");
    }

    // ---- Client: server command handler for scene transitions ----
    if (m_NetControl) {
        m_NetControl->OnServerCommandReceived = [this](const std::string& command) {
            if (command == "load_scene level2") {
                ENGINE_LOG("Client: Switching scene to level 2 as commanded by server");
                if (m_NetControl) {
                    m_NetControl->OnSceneChanged();
                }
                this->SetScene(m_DDScene2);
                // Rebind network to new scene
                if (m_NetControl) {
                    m_NetControl->BindScene(m_DDScene2, nullptr);
                }
            }
        };
        if (!isServer && std::getenv("ENGINE_BOT")) {
            ENGINE_LOG("Bot mode: Auto-connecting to local server");
            m_NetService->Connect("127.0.0.1", 7777);
        }
    }

    ENGINE_LOG("OnInit completed");
    return true;
}

// ============================================================
// OnInput
// ============================================================
void TestGame::OnInput(const Input& input)
{
    // Pass input reference to active scene dynamically
    if (m_SceneManager.GetActiveScene())
    {
        m_SceneManager.GetActiveScene()->SetInput(input);
    }

    // Networking tests
    if (m_NetService && m_NetControl) {
        if (input.IsKeyPressed(GLFW_KEY_C)) {
            if (m_NetService->GetMode() == NetworkMode::Offline) {
                m_NetService->Connect("127.0.0.1", 7777);
            }
        }
        if (input.IsKeyPressed(GLFW_KEY_H)) {
            if (m_NetService->GetMode() == NetworkMode::Offline) {
                m_NetService->Host(7777);
            }
        }
        if (input.IsKeyPressed(GLFW_KEY_M)) {
            m_NetControl->ProcessCommandString("/kick player");
        }
    }
}

// ============================================================
// OnUpdate — Option C: game code drives network + trigger logic
// ============================================================
void TestGame::OnUpdate(float deltaTime)
{
    // --- Network ticks (moved from scene to game code) ---
    if (m_NetService && m_NetService->GetMode() != NetworkMode::Offline && m_NetControl) {
        // Ensure NetworkControl is bound to current scene with current input
        Scene* activeScene = m_SceneManager.GetActiveScene();
        if (activeScene) {
            m_NetControl->BindScene(activeScene, const_cast<Input*>(activeScene->GetInput()));
        }

        float tickInterval = 1.0f / 60.0f;
        m_NetworkTimeAccumulator += std::min(deltaTime, 0.1f);
        while (m_NetworkTimeAccumulator >= tickInterval) {
            m_NetControl->Tick(tickInterval);
            m_NetworkTimeAccumulator -= tickInterval;
        }
    }

    // --- Physics configuration from network settings ---
    DataDrivenScene* activeDD = dynamic_cast<DataDrivenScene*>(m_SceneManager.GetActiveScene());
    if (activeDD && m_NetService) {
        activeDD->GetPhysics().SetPreventPlayerPlayerPushing(m_NetService->IsPlayerPushingPrevented());
        activeDD->GetPhysics().SetPlayersTransparent(m_NetService->IsPlayersTransparent());
    }

    // --- Scene update (physics, animators, etc.) ---
    m_SceneManager.Update(deltaTime);

    // --- Server: Check trigger zone for scene transition ---
    if (m_NetService && m_NetService->GetMode() == NetworkMode::Server && m_NetControl && m_TriggerID != 0) {
        if (activeDD) {
            std::vector<EntityID> players = m_NetControl->GetActivePlayerEntities();
            if (!players.empty()) {
                bool allInTrigger = true;
                for (EntityID pID : players) {
                    if (!activeDD->GetPhysics().IsColliding(pID, m_TriggerID)) {
                        allInTrigger = false;
                        break;
                    }
                }
                if (allInTrigger) {
                    ENGINE_LOG("Server: All players are in the trigger zone! Transitioning...");
                    ServerCommandPacket packet;
                    packet.header.type = PacketType::ServerCommand;
                    packet.header.tick = m_NetControl->GetServerTick();
                    strncpy(packet.command, "load_scene level2", sizeof(packet.command));
                    m_NetService->BroadcastPacket(0, &packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);
                    m_NetControl->OnSceneChanged();
                    this->SetScene(m_DDScene2);
                    m_NetControl->BindScene(m_DDScene2, nullptr);
                    m_TriggerID = 0; // Disable trigger after transition
                }
            }
        }
    }

    // --- Status text UI updates ---
    if (m_NetService) {
        if (m_NetService->GetMode() == NetworkMode::Offline) {
            if (m_StatusText) m_StatusText->Text = "Offline";
        } else if (m_NetService->GetMode() == NetworkMode::Server) {
            if (m_StatusText) m_StatusText->Text = "Hosting Server";
        } else {
            if (m_NetService->IsConnected()) {
                if (m_StatusText) m_StatusText->Text = "Connected to Server";
            } else {
                if (m_StatusText) m_StatusText->Text = "Connecting...";
            }
        }
    }

    // --- Clear UI menu once client connects ---
    if (m_NetService && m_NetService->GetMode() == NetworkMode::Client) {
        if (m_NetService->IsConnected() && !m_UIHidden) {
            Scene* scene = m_SceneManager.GetActiveScene();
            if (scene) {
                scene->registry.ClearUIElements();
            }
            m_StatusText = nullptr;
            m_UIHidden = true;
        }
    }
}

// ============================================================
// OnRender
// ============================================================
void TestGame::OnRender()
{
    m_RenderManager.BeginFrame();
    m_RenderManager.Render();
    m_RenderManager.EndFrame();
}

// ============================================================
// OnShutdown
// ============================================================
void TestGame::OnShutdown()
{
    bool isServer = IsServer();

    if (!isServer) {
        m_RenderManager.Shutdown();
    }
    m_SceneManager.Shutdown();

    if (m_DDScene1)
    {
        delete m_DDScene1;
        m_DDScene1 = nullptr;
    }
    if (m_DDScene2)
    {
        delete m_DDScene2;
        m_DDScene2 = nullptr;
    }

    m_AssetManager.Shutdown();

    if (!isServer) {
        m_FontEngine.Shutdown();
    }
    ENGINE_LOG("Shutdown");
}
