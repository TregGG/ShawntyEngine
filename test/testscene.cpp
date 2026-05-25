#include "testscene.h"
#define ENGINE_CLASS "TestScene"
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
#include <GLFW/glfw3.h>
#include "testplayer.h"

void TestScene::OnEnter()
{
    registry.Init();
    m_Physics.Init();
    m_Physics.BindRegistry(&registry);

    if (!m_Assets)
    {
        ENGINE_ERROR("OnEnter failed: no AssetManager provided");
        return;
    }

    const SpriteSheetAsset* sheet = m_Assets->GetSpriteSheet("testobj");
    const AnimationSetAsset* animSet = m_Assets->GetAnimationSet("testobj");

    // 1. Create Player (internally constructs the Player & Weapon hierarchy)
    auto playerObj = std::make_unique<TestPlayer>(this, "Player", sheet);
    m_GameObjects.push_back(std::move(playerObj));

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

    // 4. Create UI
    if (m_FontEngine) {
        // UI Panel Background
        auto panel = std::make_unique<UIPanel>(this, "MainPanel");
        panel->Position = glm::vec2(50.0f, 50.0f);
        panel->Size = glm::vec2(300.0f, 200.0f);
        panel->BackgroundColor = glm::vec4(0.2f, 0.2f, 0.2f, 0.8f);

        // UI Text
        auto text = std::make_unique<UIText>(this, "TitleText", m_FontEngine);
        text->Position = glm::vec2(20.0f, 20.0f);
        text->Text = "Shawnty Engine UI";
        text->TextColor = glm::vec3(1.0f, 0.8f, 0.0f); // Gold
        panel->AddChild(std::move(text));

        // UI Button
        auto btn = std::make_unique<UIButton>(this, "TestButton", m_EventService);
        btn->Position = glm::vec2(20.0f, 70.0f);
        btn->Size = glm::vec2(150.0f, 40.0f);
        
        auto btnText = std::make_unique<UIText>(this, "BtnText", m_FontEngine);
        btnText->Position = glm::vec2(0.0f, 0.0f);
        btnText->Size = btn->Size; // Fill button size for centering
        btnText->HorizontalAlign = TextAlignment::Center;
        btnText->VerticalAlign = VerticalAlignment::Middle;
        btnText->Text = "Click Me";
        btnText->TextColor = glm::vec3(1.0f);
        btn->AddChild(std::move(btnText));

        // UI Input Field
        auto inputF = std::make_unique<UIInputField>(this, "TestInput", m_EventService, m_FontEngine);
        inputF->Position = glm::vec2(20.0f, 130.0f);
        inputF->Size = glm::vec2(250.0f, 40.0f);
        
        UIText* rawInputText = inputF->GetTextElement();
        rawInputText->Size = inputF->Size;
        rawInputText->HorizontalAlign = TextAlignment::Left;
        rawInputText->VerticalAlign = VerticalAlignment::Middle;
        rawInputText->Position = glm::vec2(5.0f, 0.0f); // Slight left padding

        // Capture input field in button callback
        btn->OnClickCallback = [inputPtr = inputF.get()]() {
            ENGINE_LOG("Button Clicked! Input Field Text: %s", inputPtr->GetTextElement()->Text.c_str());
        };

        panel->AddChild(std::move(btn));
        panel->AddChild(std::move(inputF));

        registry.AddUIElement(std::move(panel));
    }

    // Set a reasonable view
    m_Camera.SetScale(2.0f);
    m_Camera.SetCameraPosition(0.0f, 0.0f);
}

void TestScene::OnExit()
{
    m_Physics.Shutdown();
    registry.Shutdown();
}

// void TestScene::SetInput(const Input& input)
// {
//     m_Input = &input;
// }

void TestScene::Update(float deltatime)
{
    // Query input if available
    glm::vec2 pushForce(0.0f);
    glm::vec2 cameraDir(0.0f);

    EntityID playerEntity = 0;
    if (m_GameObjects.size() > 0) playerEntity = m_GameObjects[0]->GetID();

    if (m_Input)
    {
        // Arrow keys for camera movement
        if (m_Input->IsKeyDown(GLFW_KEY_UP))    cameraDir.y += 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_DOWN))  cameraDir.y -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_LEFT))  cameraDir.x -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_RIGHT)) cameraDir.x += 1.0f;
    }

    // Pass input to player and update it
    if (!m_GameObjects.empty()) {
        if (auto* player = dynamic_cast<TestPlayer*>(m_GameObjects[0].get())) {
            player->PassInput(m_Input);
            player->Update(deltatime);
        }
    }

    m_Physics.Update(deltatime);

    // Apply input-driven camera movement
    glm::vec2 camPos = m_Camera.GetCameraPosition();
    m_Camera.SetCameraPosition(
        camPos.x + cameraDir.x * m_MoveSpeed * deltatime,
        camPos.y + cameraDir.y * m_MoveSpeed * deltatime
    );

    // Animators Update
    for (EntityID e : registry.ViewAnimators()) {
        auto& animator = registry.GetComponent<AnimatorComponent>(e);
        if (animator.IsActive()) {
            animator.Update(deltatime);
        }
    }
    
    // Transform Hierarchy Update (rudimentary transform system)
    // Manual transform update loop removed! The engine computes this automatically on the fly via GetWorldPosition()
    // SHAPE CAST DEMONSTRATION
    m_TestLines.clear();
    m_TestRects.clear();
    if (playerEntity != 0 && registry.HasComponent<TransformComponent>(playerEntity)) {
        glm::vec2 pPos = registry.GetComponent<TransformComponent>(playerEntity).position;
        
        // Ignore UI layer AND Player layer so we don't immediately hit ourselves!
        uint32_t testingMask = ~( (1 << static_cast<int>(Layer::UI)) | (1 << static_cast<int>(Layer::Player)) ); 

        // 1. Box Cast to the right
        RaycastHit boxHit;
        glm::vec2 boxDir(1.0f, 0.0f);
        glm::vec2 boxSize(1.0f, 1.0f);
        float boxLen = 15.0f;
        
        if (BOX_CAST(pPos, pPos + boxDir * boxLen, boxSize, boxHit, testingMask)) {
            m_TestLines.push_back({pPos, boxHit.point, glm::vec3(1.0f, 0.5f, 0.0f)}); // Orange line
            m_TestRects.push_back({glm::vec3(boxHit.point, 0.0f), boxSize, glm::vec3(1.0f, 0.5f, 0.0f)}); // Orange box
        } else {
            m_TestLines.push_back({pPos, pPos + boxDir * boxLen, glm::vec3(0.0f, 1.0f, 0.0f)});
            m_TestRects.push_back({glm::vec3(pPos + boxDir * boxLen, 0.0f), boxSize, glm::vec3(0.0f, 1.0f, 0.0f)});
        }

        // 2. Circle Cast upwards
        RaycastHit circHit;
        glm::vec2 circDir(0.0f, 1.0f);
        float circRadius = 0.5f;
        float circLen = 10.0f;
        
        if (CIRCLE_CAST(pPos, pPos + circDir * circLen, circRadius, circHit, testingMask)) {
            m_TestLines.push_back({pPos, circHit.point, glm::vec3(0.0f, 0.5f, 1.0f)}); // Blue line
            m_TestRects.push_back({glm::vec3(circHit.point, 0.0f), glm::vec2(circRadius * 2.0f), glm::vec3(0.0f, 0.5f, 1.0f)}); // Blue square
        } else {
            m_TestLines.push_back({pPos, pPos + circDir * circLen, glm::vec3(0.0f, 1.0f, 0.0f)});
            m_TestRects.push_back({glm::vec3(pPos + circDir * circLen, 0.0f), glm::vec2(circRadius * 2.0f), glm::vec3(0.0f, 1.0f, 0.0f)});
        }
    }
}

void TestScene::BuildDebugRenderables(std::vector<DebugRect>& outDebugRects) const
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
        
        // Render triggers as RED instead of yellow
        glm::vec3 cColor = col.IsTrigger() ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        outDebugRects.push_back({pos, size, cColor});
    }
    
    // Append the custom shape cast test rectangles
    for (const auto& r : m_TestRects) {
        outDebugRects.push_back(r);
    }
#endif
}

void TestScene::BuildDebugLines(std::vector<DebugLine>& outDebugLines) const
{
#ifdef ENGINE_DEBUG
    outDebugLines = m_TestLines;
#endif
}
