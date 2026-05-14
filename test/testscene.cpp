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

void TestScene::OnEnter()
{
    m_Registry.Init();
    m_Physics.Init();

    if (!m_Assets)
    {
        ENGINE_ERROR("OnEnter failed: no AssetManager provided");
        return;
    }

    const SpriteSheetAsset* sheet = m_Assets->GetSpriteSheet("testobj");
    const AnimationSetAsset* animSet = m_Assets->GetAnimationSet("testobj");

    // 1. Create Player
    auto playerObj = std::make_unique<GameObject>("Player");
    m_Registry.Create(EntityCategory::Player, "Player", "TestScene");
    playerObj->GetTransform().position = glm::vec2(0.0f, 5.0f);
    playerObj->GetTransform().size = glm::vec2(1.0f, 1.0f);
    playerObj->SetLayer(Layer::Player);

    auto pRenderer = std::make_unique<SpriteRenderer2D>();
    pRenderer->SetSpriteSheet(sheet);
    pRenderer->SetFrameIndex(0);
    playerObj->AddComponent(std::move(pRenderer));

    auto pCollider = std::make_unique<ColliderComponent>();
    pCollider->SetAutoBounds(true);
    m_Physics.RegisterCollider(pCollider.get());
    playerObj->AddComponent(std::move(pCollider));

    auto pRb = std::make_unique<RigidBodyComponent>();
    pRb->SetType(BodyType::Dynamic);
    pRb->SetDrag(2.0f); // Slight drag for air control
    pRb->SetUseGravity(true);
    pRb->SetGravityScale(2.0f); // Fall faster
    m_Physics.RegisterRigidBody(pRb.get());
    playerObj->AddComponent(std::move(pRb));

    m_GameObjects.push_back(std::move(playerObj));

    // 2. Create Ground
    auto groundObj = std::make_unique<GameObject>("Ground");
    m_Registry.Create(EntityCategory::Environment, "Ground", "TestScene");
    groundObj->GetTransform().position = glm::vec2(0.0f, -3.0f);
    groundObj->GetTransform().size = glm::vec2(15.0f, 1.0f);
    groundObj->SetLayer(Layer::Background);

    auto gRenderer = std::make_unique<SpriteRenderer2D>();
    gRenderer->SetSpriteSheet(sheet);
    gRenderer->SetFrameIndex(0);
    groundObj->AddComponent(std::move(gRenderer));

    auto gCollider = std::make_unique<ColliderComponent>();
    gCollider->SetAutoBounds(true);
    m_Physics.RegisterCollider(gCollider.get());
    groundObj->AddComponent(std::move(gCollider));

    auto gRb = std::make_unique<RigidBodyComponent>();
    gRb->SetType(BodyType::Static);
    gRb->SetElasticity(0.0f);
    m_Physics.RegisterRigidBody(gRb.get());
    groundObj->AddComponent(std::move(gRb));

    m_GameObjects.push_back(std::move(groundObj));

    // 3. Create Trampoline
    auto trampObj = std::make_unique<GameObject>("Trampoline");
    m_Registry.Create(EntityCategory::Environment, "Trampoline", "TestScene");
    trampObj->GetTransform().position = glm::vec2(0.0f, -1.0f);
    trampObj->GetTransform().size = glm::vec2(3.0f, 0.5f);
    trampObj->SetLayer(Layer::Foreground);

    auto tRenderer = std::make_unique<SpriteRenderer2D>();
    tRenderer->SetSpriteSheet(sheet);
    tRenderer->SetFrameIndex(0);
    trampObj->AddComponent(std::move(tRenderer));

    auto tCollider = std::make_unique<ColliderComponent>();
    tCollider->SetAutoBounds(true);
    m_Physics.RegisterCollider(tCollider.get());
    trampObj->AddComponent(std::move(tCollider));

    auto tRb = std::make_unique<RigidBodyComponent>();
    tRb->SetType(BodyType::Static);
    tRb->SetElasticity(2.5f); // Super bouncy!
    m_Physics.RegisterRigidBody(tRb.get());
    trampObj->AddComponent(std::move(tRb));

    m_GameObjects.push_back(std::move(trampObj));

    // Set a reasonable view
    m_Camera.SetScale(2.0f);
    m_Camera.SetCameraPosition(0.0f, 0.0f);
}

void TestScene::OnExit()
{
    m_Physics.Shutdown();
    m_Registry.Shutdown();
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

    if (m_Input)
    {
        // A/D for lateral movement
        if (m_Input->IsKeyDown(GLFW_KEY_A)) pushForce.x -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_D)) pushForce.x += 1.0f;
        
        static bool spacePressed = false;
        if (m_Input->IsKeyDown(GLFW_KEY_SPACE)) {
            if (!spacePressed) {
                // Jump!
                if (m_GameObjects.size() > 0) {
                    RigidBodyComponent* rb = m_GameObjects[0]->GetComponent<RigidBodyComponent>();
                    if (rb) rb->SetVelocity(glm::vec2(rb->GetVelocity().x, 15.0f));
                }
                spacePressed = true;
            }
        } else {
            spacePressed = false;
        }

        // Arrow keys for camera movement
        if (m_Input->IsKeyDown(GLFW_KEY_UP))    cameraDir.y += 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_DOWN))  cameraDir.y -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_LEFT))  cameraDir.x -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_RIGHT)) cameraDir.x += 1.0f;
    }

    // Apply input-driven movement to first GameObject (controllable sprite)
    if (m_GameObjects.size() > 0)
    {
        RigidBodyComponent* rb = m_GameObjects[0]->GetComponent<RigidBodyComponent>();
        if (rb) {
            rb->AddForce(pushForce * 50.0f);
        }

        m_Physics.Update(deltatime);
    }

    // Apply input-driven camera movement
    glm::vec2 camPos = m_Camera.GetCameraPosition();
    m_Camera.SetCameraPosition(
        camPos.x + cameraDir.x * m_MoveSpeed * deltatime,
        camPos.y + cameraDir.y * m_MoveSpeed * deltatime
    );

    // Update all game objects
    for (auto& obj : m_GameObjects)
    {
        if (obj && obj->IsActive())
            obj->Update(deltatime);
    }
    
    // SHAPE CAST DEMONSTRATION
    m_TestLines.clear();
    m_TestRects.clear();
    if (m_GameObjects.size() > 0) {
        glm::vec2 pPos = m_GameObjects[0]->GetTransform().position;
        
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
    for (const auto& obj : m_GameObjects) 
    {
        if (!obj || !obj->IsActive()) continue;
        
        // Temporarily bypassing const to pull the active component ptr mapping dynamically 
        auto* objPtr = const_cast<GameObject*>(obj.get());
        if (auto col = objPtr->GetComponent<ColliderComponent>()) 
        {
            auto b = col->GetBounds();
            glm::vec2 size(b.maxX - b.minX, b.maxY - b.minY);
            glm::vec3 pos(b.minX + size.x * 0.5f, b.minY + size.y * 0.5f, 0.0f);
            
            // Render triggers as RED instead of yellow
            glm::vec3 cColor = col->IsTrigger() ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
            outDebugRects.push_back({pos, size, cColor});
        }
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
