#include "testscene2.h"
#define ENGINE_CLASS "TestScene2"
#include "../core/enginedebug.h"
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#include "../assets/assetmanager.h"
#include "../core/input.h"
#include "../objects/gameobject.h"
#include "../objects/components/animator.h"
#include "../objects/components/spriterenderer2d.h"
#include "../objects/components/collidercomponent.h"
#include "../objects/components/rigidbodycomponent.h"
#include "../services/base/raycast.h"
#include "../services/networkservice.h"
#include "../services/networkcontrol.h"
#include <GLFW/glfw3.h>
#include <algorithm>

void TestScene2::OnEnter()
{
    registry.Init();
    m_Physics.Init();
    m_Physics.BindRegistry(&registry);

    if (m_NetControl) {
        m_NetControl->BindScene(this, const_cast<Input*>(m_Input));
    }

    if (!m_Assets)
    {
        ENGINE_ERROR("OnEnter failed: no AssetManager provided");
        return;
    }

    const SpriteSheetAsset* sheet = m_Assets->GetSpriteSheet("testobj");

    // 2. Create Ground
    auto groundObj = std::make_unique<GameObject>(this, "Ground");
    EntityID gID = groundObj->GetID();

    TransformComponent gt;
    gt.position = glm::vec2(0.0f, -3.0f);
    gt.size = glm::vec2(15.0f, 1.0f);
    registry.AddComponent<TransformComponent>(gID, gt);

    SpriteComponent2D gs;
    gs.spriteSheet = sheet;
    gs.frameIndex = 0;
    gs.layer = Layer::Background;
    registry.AddComponent<SpriteComponent2D>(gID, gs);

    ColliderComponent gc;
    gc.SetAutoBounds(true);
    registry.AddComponent<ColliderComponent>(gID, gc);

    RigidBodyComponent grb;
    grb.SetType(BodyType::Static);
    grb.SetElasticity(0.0f);
    registry.AddComponent<RigidBodyComponent>(gID, grb);

    m_GameObjects.push_back(std::move(groundObj));

    // 3. Create Trampoline
    auto trampObj = std::make_unique<GameObject>(this, "Trampoline");
    EntityID tID = trampObj->GetID();

    TransformComponent tt;
    tt.position = glm::vec2(0.0f, -1.0f);
    tt.size = glm::vec2(3.0f, 0.5f);
    registry.AddComponent<TransformComponent>(tID, tt);

    SpriteComponent2D ts;
    ts.spriteSheet = sheet;
    ts.frameIndex = 0;
    ts.layer = Layer::Foreground;
    registry.AddComponent<SpriteComponent2D>(tID, ts);

    ColliderComponent tc;
    tc.SetAutoBounds(true);
    registry.AddComponent<ColliderComponent>(tID, tc);

    RigidBodyComponent trb;
    trb.SetType(BodyType::Static);
    trb.SetElasticity(2.5f); // Super bouncy!
    registry.AddComponent<RigidBodyComponent>(tID, trb);
    
    m_GameObjects.push_back(std::move(trampObj));

    // 4. Create UI (if not server and not already connected)
    bool isServer = (m_NetService && m_NetService->GetMode() == NetworkMode::Server);
    bool isConnected = (m_NetService && m_NetService->IsConnected());
    if (m_FontEngine && !isServer && !isConnected) {
        // UI Panel Background
        auto panel = std::make_unique<UIPanel>(this, "MainPanel");
        panel->Position = glm::vec2(50.0f, 50.0f);
        panel->Size = glm::vec2(300.0f, 250.0f);
        panel->BackgroundColor = glm::vec4(0.2f, 0.2f, 0.2f, 0.8f);

        // UI Text
        auto text = std::make_unique<UIText>(this, "TitleText", m_FontEngine);
        text->Position = glm::vec2(20.0f, 10.0f);
        text->Text = "Multiplayer Test - Level 2";
        text->TextColor = glm::vec3(1.0f, 0.8f, 0.0f); // Gold
        panel->AddChild(std::move(text));

        // IP Input Field
        auto inputF = std::make_unique<UIInputField>(this, "IPInput", m_EventService, m_FontEngine);
        inputF->Position = glm::vec2(20.0f, 60.0f);
        inputF->Size = glm::vec2(250.0f, 40.0f);
        
        UIText* rawInputText = inputF->GetTextElement();
        rawInputText->Size = inputF->Size;
        rawInputText->HorizontalAlign = TextAlignment::Left;
        rawInputText->VerticalAlign = VerticalAlignment::Middle;
        rawInputText->Position = glm::vec2(5.0f, 0.0f);
        rawInputText->Text = "127.0.0.1"; // Default IP

        // Host Button
        auto hostBtn = std::make_unique<UIButton>(this, "HostButton", m_EventService);
        hostBtn->Position = glm::vec2(20.0f, 120.0f);
        hostBtn->Size = glm::vec2(120.0f, 40.0f);
        
        auto hostText = std::make_unique<UIText>(this, "HostBtnText", m_FontEngine);
        hostText->Position = glm::vec2(0.0f, 0.0f);
        hostText->Size = hostBtn->Size;
        hostText->HorizontalAlign = TextAlignment::Center;
        hostText->VerticalAlign = VerticalAlignment::Middle;
        hostText->Text = "Host";
        hostText->TextColor = glm::vec3(1.0f);
        hostBtn->AddChild(std::move(hostText));

        // Join Button
        auto joinBtn = std::make_unique<UIButton>(this, "JoinButton", m_EventService);
        joinBtn->Position = glm::vec2(150.0f, 120.0f);
        joinBtn->Size = glm::vec2(120.0f, 40.0f);
        
        auto joinText = std::make_unique<UIText>(this, "JoinBtnText", m_FontEngine);
        joinText->Position = glm::vec2(0.0f, 0.0f);
        joinText->Size = joinBtn->Size;
        joinText->HorizontalAlign = TextAlignment::Center;
        joinText->VerticalAlign = VerticalAlignment::Middle;
        joinText->Text = "Join";
        joinText->TextColor = glm::vec3(1.0f);
        joinBtn->AddChild(std::move(joinText));

        // Status Text
        auto statusTxt = std::make_unique<UIText>(this, "StatusText", m_FontEngine);
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
        
        registry.AddUIElement(std::move(panel));
    }

    // Set a reasonable view
    m_Camera.SetScale(2.0f);
    m_Camera.SetCameraPosition(0.0f, 0.0f);
}

void TestScene2::OnExit()
{
    m_Physics.Shutdown();
    registry.Shutdown();
}

void TestScene2::Update(float deltatime)
{
    // Ensure m_NetControl has the latest non-null m_Input pointer
    if (m_NetControl) {
        m_NetControl->BindScene(this, const_cast<Input*>(m_Input));
    }

    // Clamp deltatime to avoid huge bursts of ticks (spiral of death) when window loses focus
    deltatime = std::min(deltatime, 0.1f);

    glm::vec2 cameraDir(0.0f);

    if (m_Input)
    {
        // Arrow keys for camera movement
        if (m_Input->IsKeyDown(GLFW_KEY_UP))    cameraDir.y += 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_DOWN))  cameraDir.y -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_LEFT))  cameraDir.x -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_RIGHT)) cameraDir.x += 1.0f;
        
        if (m_Input->IsKeyPressed(GLFW_KEY_TAB)) {
            m_UIHidden = !m_UIHidden;
        }
    }

    // Fixed 60Hz tick accumulator
    float tickInterval = 1.0f / 60.0f;
    m_TimeAccumulator += deltatime;
    while (m_TimeAccumulator >= tickInterval)
    {
        if (m_NetService && m_NetService->GetMode() != NetworkMode::Offline) {
            if (m_NetControl) m_NetControl->Tick(tickInterval);
        }

        if (m_NetService) {
            m_Physics.SetPreventPlayerPlayerPushing(m_NetService->IsPlayerPushingPrevented());
            m_Physics.SetPlayersTransparent(m_NetService->IsPlayersTransparent());
        }

        m_Physics.Update(tickInterval);
        
        // ECS components sleeping logic has been removed as View is unsupported
        
        m_TimeAccumulator -= tickInterval;
    }

    // Manual camera movement via arrow keys
    glm::vec2 newPos = m_Camera.GetCameraPosition() + cameraDir * m_MoveSpeed * deltatime;
    m_Camera.SetCameraPosition(newPos.x, newPos.y);

    if (m_NetService && m_NetService->GetMode() == NetworkMode::Client) {
        if (m_NetService->IsConnected() && !m_UIHidden) {
            registry.ClearUIElements();
            m_StatusText = nullptr;
            m_UIHidden = true;
        }
    }

    // Status logic
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

    // Animators
    for (EntityID e : registry.ViewAnimators()) {
        auto& animator = registry.GetComponent<AnimatorComponent>(e);
        if (animator.IsActive()) {
            animator.Update(deltatime);
        }
    }

    registry.Update(deltatime);
}

void TestScene2::BuildDebugRenderables(std::vector<DebugRect>& outDebugRects) const
{
    outDebugRects.insert(outDebugRects.end(), m_TestRects.begin(), m_TestRects.end());
}

void TestScene2::BuildDebugLines(std::vector<DebugLine>& outDebugLines) const
{
    outDebugLines.insert(outDebugLines.end(), m_TestLines.begin(), m_TestLines.end());
    
    // Draw crosshair at origin
    outDebugLines.push_back({glm::vec2(-0.5f, 0.0f), glm::vec2(0.5f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)});
    outDebugLines.push_back({glm::vec2(0.0f, -0.5f), glm::vec2(0.0f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f)});
}
