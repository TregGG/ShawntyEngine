#include "testgame.h"

#include "../core/engine.h"
#include "../core/input.h"
#include "../assets/assetmanager.h"
#include "../core/enginedebug.h"
#include "../services/networkservice.h"
#include "testnetworkcontrol.h"
#include "../levels/datadrivenscene.h"
#include "../core/projectpaths.h"
#include "../core/endpoint.h"

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
    bool isConnected = (m_NetService && m_NetService->IsConnected());

    if (isConnected) return;

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
    rawInputText->Text = "127.0.0.1:7777"; // Default address

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
            Join("127.0.0.1:7777");
        }
    };

    joinBtn->OnClickCallback = [this, inputPtr = inputF.get()]() {
        if (m_NetService) {
            if (!Join(inputPtr->GetTextElement()->Text)) SetStatus("Enter IP:port or IP:port/CODE");
        }
    };

    panel->AddChild(std::move(inputF));
    panel->AddChild(std::move(hostBtn));
    panel->AddChild(std::move(joinBtn));

    scene->registry.AddUIElement(std::move(panel));
}

bool TestGame::Join(const std::string& address)
{
    std::string host, code;
    int port = 0;
    if (!m_NetService || m_NetService->GetMode() != NetworkMode::Offline) return false;
    if (!ParseEndpoint(address, host, port, code)) {
        ENGINE_WARN("Invalid server address '%s'. Use IP:port or IP:port/CODE", address.c_str());
        return false;
    }
    m_Content.SetJoinCode(code);
    return m_NetService->Connect(host, port);
}

void TestGame::SetStatus(const std::string& text)
{
    ENGINE_LOG("%s", text.c_str());
    if (m_StatusText) m_StatusText->Text = text;
}

// Drops everything from the current project and starts another one.
void TestGame::SwitchProject(const std::string& root, const std::string& scenePath)
{
    m_StatusText = nullptr;
    if (m_NetControl) {
        m_NetControl->OnSceneChanged();
        m_NetControl->BindScene(nullptr, nullptr);
    }
    SetScene(nullptr);
    for (auto& [path, scene] : m_Scenes) delete scene;
    m_Scenes.clear();

    ProjectPaths::SetRoot(root);
    m_AssetManager.Reload(root);
    DataDrivenScene* scene = GetOrCreateScene(scenePath);
    SetScene(scene);
    if (m_NetControl) m_NetControl->BindScene(scene, nullptr);
}

DataDrivenScene* TestGame::GetOrCreateScene(const std::string& rawPath) {
    const std::string path = ProjectPaths::Resolve(rawPath);
    auto it = m_Scenes.find(path);
    if (it != m_Scenes.end()) {
        return it->second;
    }

    ENGINE_LOG("Loading dynamic scene on client: %s", path.c_str());
    DataDrivenScene* scene = new DataDrivenScene(&m_AssetManager, path, &m_FontEngine, m_EventService);
    scene->SetScriptsEnabled(!m_Remote);
    scene->OnSceneReloadedCallback = [this, path, scene]() {
        this->HandleSceneReloaded(path, scene);
    };

    scene->registry.SetUIActionCallback([this, scene](const std::string& action, const std::string& target) {
        if (m_Remote && (action == "Host" || action == "Join")) {
            return; // a downloaded project may not start servers or redirect players
        }
        if (action == "Host") {
            ENGINE_LOG("Host Action triggered. Launching dedicated server...");
#ifdef _WIN32
            std::system("start bin\\framework.exe --server");
#elif defined(__APPLE__)
            std::system("open -n ./bin/framework --args --server");
#else
            std::system("./bin/framework --server &");
#endif
            Join("127.0.0.1:7777");
        } else if (action == "Join") {
            std::string address = "127.0.0.1:7777";
            if (!target.empty() && scene) {
                if (auto inputF = scene->registry.FindUIElementRecursive(target)) {
                    if (auto uiInput = dynamic_cast<UIInputField*>(inputF)) {
                        if (uiInput->GetTextElement()) address = uiInput->GetTextElement()->Text;
                    }
                }
            }
            Join(address);
        } else if (action == "Quit") {
            ENGINE_LOG("Quit Action triggered.");
            exit(0);
        } else if (action == "ToggleActive") {
            if (scene) {
                if (auto targetElement = scene->registry.FindUIElementRecursive(target)) {
                    targetElement->Active = !targetElement->Active;
                    ENGINE_LOG("ToggleActive triggered for %s. Active is now %d", target.c_str(), targetElement->Active);
                } else {
                    ENGINE_LOG("ToggleActive failed: Could not find UI element with Editor ID %s", target.c_str());
                }
            }
        }
    });

    m_Scenes[path] = scene;
    return scene;
}

void TestGame::HandleSceneReloaded(const std::string& path, DataDrivenScene* scene) {
    ENGINE_LOG("Client: Scene '%s' reloaded locally!", path.c_str());
    if (m_NetControl && m_SceneManager.GetActiveScene() == scene) {
        m_NetControl->OnSceneChanged();
        if (m_NetService && m_NetService->GetMode() == NetworkMode::Offline) {
            m_NetControl->SpawnLocalPlayer();
        }
    }
}

// ============================================================
// OnInit
// ============================================================
bool TestGame::OnInit()
{
    ENGINE_LOG("OnInit");

    const std::string compiledRoot = ProjectPaths::Root();

    if (!m_AssetManager.Initialize(compiledRoot, false))
    {
        ENGINE_ERROR("AssetManager failed to initialize");
        return false;
    }

    ENGINE_LOG("AssetManager initialized, initializing RenderManager");
    if (!m_RenderManager.Initialize())
    {
        ENGINE_ERROR("RenderManager failed to initialize");
        return false;
    }

    ENGINE_LOG("RenderManager initialized, creating FontEngine");
    m_FontEngine.Init();
    m_FontEngine.LoadFont("assets/comic.ttf", 24);

    // Register global change scene callback for Python scripts
    g_ChangeSceneCallback = [this](const std::string& scenePath) {
        DataDrivenScene* targetScene = GetOrCreateScene(scenePath);
        if (targetScene) {
            if (m_SceneManager.GetActiveScene() == targetScene) {
                targetScene->Reload();
            } else {
                this->SetScene(targetScene);
                if (m_NetControl) {
                    m_NetControl->OnSceneChanged();
                    m_NetControl->BindScene(targetScene, nullptr);
                    m_NetControl->SpawnLocalPlayer();
                }
            }
        }
    };

    // ---- Create Initial Scene from JSON ----
    DataDrivenScene* initialScene = GetOrCreateScene(ProjectPaths::IsDefault()
        ? "test_compiled/scenes/mainmenu.scene" : "scenes/main.scene");
    m_SceneManager.SetInitialScene(initialScene);
    m_RenderManager.BindScene(initialScene);

    // ---- Wire networking (Option C: game code handles this) ----
    if (m_NetControl) {
        m_NetControl->BindScene(initialScene, nullptr); // Input set later in OnInput
        
        m_NetControl->OnClientConnectedCallback = [this](const std::string& sceneName) {
            ENGINE_LOG("Connected! Server requests scene: %s", sceneName.c_str());
            if (!IsSafeContentPath(sceneName) || sceneName.compare(0, 7, "scenes/") != 0) {
                ENGINE_ERROR("Server requested an invalid scene");
                m_NetService->Disconnect();
                m_ReturnToMenu = true;
                return;
            }
            DataDrivenScene* targetScene = GetOrCreateScene(sceneName);
            if (targetScene) {
                if (m_SceneManager.GetActiveScene() == targetScene) {
                    targetScene->Reload();
                } else {
                    this->SetScene(targetScene);
                }
                m_NetControl->BindScene(targetScene, nullptr);
                m_NetControl->SpawnLocalPlayer();
            } else {
                ENGINE_ERROR("Unknown scene requested by server: %s", sceneName.c_str());
            }
        };
    }

    m_MenuRoot = ProjectPaths::Root();
    if (m_NetControl && m_NetService) {
        m_NetControl->SetContentClient(&m_Content);
        m_NetControl->SetAssets(&m_AssetManager);
        m_NetService->OnConnected = [this]() {
            SetStatus("Joining...");
            m_Content.OnConnected(*m_NetService);
        };
        m_NetService->OnDisconnected = [this](uint32_t reason) {
            m_Content.OnDisconnected();
            ENGINE_WARN("%s", DescribeDisconnect(reason));
            m_ReturnToMenu = m_Remote;
        };
        m_Content.OnProgress = [this](size_t done, size_t total) {
            if (total > 0) SetStatus("Downloading " + std::to_string(done) + "/" + std::to_string(total));
        };
        m_Content.OnError = [](const std::string& error) {
            (void)error;
            ENGINE_ERROR("%s", error.c_str());
        };
        m_Content.OnContent = [this](const std::string& root) {
            m_Remote = true;
            SwitchProject(root, "scenes/main.scene");
            m_Content.SendReady(*m_NetService);
        };
    }

    // UI Actions are now registered in GetOrCreateScene

    // ---- Client: server command handler for other commands if needed ----
    if (m_NetControl) {
        m_NetControl->OnServerCommandReceived = [this](const std::string& command) {
            // Can add other commands here later
            ENGINE_LOG("Received server command: %s", command.c_str());
        };
        if (std::getenv("ENGINE_BOT") && m_AutoConnect.empty()) {
            ENGINE_LOG("Bot mode: Auto-connecting to local server");
            Join("127.0.0.1:7777");
        }
    }

    if (!m_AutoConnect.empty()) Join(m_AutoConnect);

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
                Join("127.0.0.1:7777");
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
    // Scene update (physics, animators, etc.)
    if (m_ReturnToMenu) {
        m_ReturnToMenu = false;
        m_Remote = false;
        m_UIHidden = false;
        SwitchProject(m_MenuRoot, m_MenuRoot == ProjectPaths::kDefaultRoot ? "test_compiled/scenes/mainmenu.scene" : "scenes/main.scene");
    }

    m_SceneManager.Update(deltaTime);

    // --- Network ticks (moved from scene to game code) ---
    if (m_NetService && m_NetService->GetMode() != NetworkMode::Offline && m_NetControl) {
        // Ensure NetworkControl is bound to current scene with current input
        Scene* activeScene = m_SceneManager.GetActiveScene();
        if (activeScene) {
            m_NetControl->BindScene(activeScene, const_cast<Input*>(activeScene->GetInput()));
        }

        float tickInterval = 1.0f / 60.0f; // 60 ticks per second for lower input latency
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
        
        // Enable UDP hot reload listener only when offline (single player)
        activeDD->SetUdpListenerEnabled(m_NetService->GetMode() == NetworkMode::Offline);
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
        if (m_NetService->IsConnected() && !m_UIHidden && !m_Remote) {
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
    m_RenderManager.Shutdown();
    m_SceneManager.Shutdown();

    for (auto& [path, scene] : m_Scenes) {
        delete scene;
    }
    m_Scenes.clear();

    m_AssetManager.Shutdown();

    m_FontEngine.Shutdown();
    ENGINE_LOG("Shutdown");
}
