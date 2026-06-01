#include "testscene1.h"
#define ENGINE_CLASS "TestScene1"
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
#include "../services/networkservice.h"
#include "../services/networkcontrol.h"
#include <GLFW/glfw3.h>
#include <algorithm>

#include "../objects/ui/uipanel.h"
#include "../objects/ui/uitext.h"
#include "../objects/ui/uibutton.h"
#include "../objects/ui/uiinputfield.h"
#include "../render/fontengine.h"
#include "../services/base/eventservice.h"

void TestScene1::OnEnter()
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

    // 1. Create Ground
    auto groundObj = std::make_unique<GameObject>(this, "Ground");
    EntityID gID = groundObj->GetID();

    TransformComponent gt;
    gt.position = glm::vec2(0.0f, -3.0f);
    gt.size = glm::vec2(25.0f, 1.0f);
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

    // 2. Create Level Trigger
    m_TriggerID = CreateEntity(EntityCategory::Environment, "SceneTrigger");

    TransformComponent tt;
    tt.position = glm::vec2(5.0f, -1.0f); // trigger box to the right
    tt.size = glm::vec2(2.0f, 2.0f);
    registry.AddComponent<TransformComponent>(m_TriggerID, tt);

    SpriteComponent2D ts;
    ts.spriteSheet = nullptr; // invisible sprite
    ts.layer = Layer::Foreground;
    registry.AddComponent<SpriteComponent2D>(m_TriggerID, ts);

    ColliderComponent tc;
    tc.SetAutoBounds(true);
    tc.SetTrigger(true);
    registry.AddComponent<ColliderComponent>(m_TriggerID, tc);

    // 3. Create UI (if not server and not already connected)
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
        text->Text = "Multiplayer Test - Level 1";
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

void TestScene1::OnExit()
{
    m_Physics.Shutdown();
    registry.Shutdown();
}

void TestScene1::Update(float deltatime)
{
    // Ensure m_NetControl has the latest input pointer
    if (m_NetControl) {
        m_NetControl->BindScene(this, const_cast<Input*>(m_Input));
    }

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
        m_TimeAccumulator -= tickInterval;
    }

    // Server Trigger Check: Check if all active players are in the trigger zone
    if (m_NetService && m_NetService->GetMode() == NetworkMode::Server && m_NetControl) {
        std::vector<EntityID> players = m_NetControl->GetActivePlayerEntities();
        if (!players.empty()) {
            bool allInTrigger = true;
            for (EntityID pID : players) {
                if (!m_Physics.IsColliding(pID, m_TriggerID)) {
                    allInTrigger = false;
                    break;
                }
            }
            if (allInTrigger && OnAllPlayersInTrigger) {
                ENGINE_LOG("Server: All players are in the trigger zone! Transitioning...");
                OnAllPlayersInTrigger();
            }
        }
    }

    // Camera free movement
    glm::vec2 newPos = m_Camera.GetCameraPosition() + cameraDir * m_MoveSpeed * deltatime;
    m_Camera.SetCameraPosition(newPos.x, newPos.y);

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

    // Clear UI menu once client connects
    if (m_NetService && m_NetService->GetMode() == NetworkMode::Client) {
        if (m_NetService->IsConnected() && !m_UIHidden) {
            registry.ClearUIElements();
            m_StatusText = nullptr;
            m_UIHidden = true;
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

void TestScene1::BuildDebugRenderables(std::vector<DebugRect>& outDebugRects) const
{
#ifdef ENGINE_DEBUG
    outDebugRects.clear();
    for (EntityID e : registry.ViewPhysicsObjects()) 
    {
        const auto& col = registry.GetComponent<ColliderComponent>(e);
        const auto& trans = registry.GetComponent<TransformComponent>(e);
        
        auto b = col.GetBounds(trans);
        glm::vec2 size(b.maxX - b.minX, b.maxY - b.minY);
        glm::vec3 pos(b.minX + size.x * 0.5f, b.minY + size.y * 0.5f, 0.0f);
        
        glm::vec3 cColor = col.IsTrigger() ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        outDebugRects.push_back({pos, size, cColor});
    }
#else
    (void)outDebugRects;
#endif
}

void TestScene1::BuildDebugLines(std::vector<DebugLine>& outDebugLines) const
{
    // Draw crosshair at origin
    outDebugLines.push_back({glm::vec2(-0.5f, 0.0f), glm::vec2(0.5f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)});
    outDebugLines.push_back({glm::vec2(0.0f, -0.5f), glm::vec2(0.0f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f)});
}
