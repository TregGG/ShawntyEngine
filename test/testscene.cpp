#include "testscene.h"
#define ENGINE_CLASS "TestScene"
#include "../core/enginedebug.h"
#include <glm/vec2.hpp>
#include "../assets/assetmanager.h"
#include "../core/input.h"
#include "../objects/gameobject.h"
#include "../objects/components/animator.h"
#include "../objects/components/spriterenderer2d.h"
#include "../objects/components/collidercomponent.h"
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

    ENGINE_LOG("OnEnter: getting sprite sheet and animation set");
    // Get sprite sheet and animation set for "testobj"
    const SpriteSheetAsset* sheet = nullptr;
    const AnimationSetAsset* animSet = nullptr;
    try {
        sheet = m_Assets->GetSpriteSheet("testobj");
        ENGINE_LOG("OnEnter: got sprite sheet");
        animSet = m_Assets->GetAnimationSet("testobj");
        ENGINE_LOG("OnEnter: got animation set");
    } catch (const std::exception& e) {
        ENGINE_ERROR("OnEnter: failed to load assets for 'testobj': %s", e.what());
        return;
    }

    ENGINE_LOG("OnEnter: creating GameObject");
    // Create a GameObject and add AnimatorComponent and SpriteRenderer2D
    auto gameObj = std::make_unique<GameObject>("TestSprite");
    m_Registry.Create(EntityCategory::Player, "TestSprite", "TestScene");
    gameObj->GetTransform().position = glm::vec2(0.0f, 0.0f);
    gameObj->GetTransform().size = glm::vec2(2.0f, 2.0f);

    ENGINE_LOG("OnEnter: creating AnimatorComponent");
    auto animator = std::make_unique<AnimatorComponent>();
    animator->BindAnimationSet(animSet, sheet);
    ENGINE_LOG("OnEnter: animator bound, playing idle");
    animator->Play("idle", true);
    ENGINE_LOG("OnEnter: animator playing");
    gameObj->AddComponent(std::move(animator));

    ENGINE_LOG("OnEnter: creating SpriteRenderer2D");
    auto spriteRenderer = std::make_unique<SpriteRenderer2D>();
    spriteRenderer->SetSpriteSheet(sheet);
    spriteRenderer->SetFrameIndex(0);
    gameObj->AddComponent(std::move(spriteRenderer));

    auto playerCollider = std::make_unique<ColliderComponent>(glm::vec2(0.0f), glm::vec2(2.0f, 2.0f));
    m_Physics.RegisterCollider(playerCollider.get());
    gameObj->AddComponent(std::move(playerCollider));

    m_GameObjects.push_back(std::move(gameObj));

    // Create trigger zone object
    ENGINE_LOG("OnEnter: creating trigger object");
    auto triggerObj = std::make_unique<GameObject>("trigger_zone");
    m_Registry.Create(EntityCategory::Environment, "trigger_zone", "TestScene");
    triggerObj->GetTransform().position = glm::vec2(-3.0f, 0.0f);
    triggerObj->GetTransform().size = glm::vec2(1.5f, 1.5f);

    auto triggerRenderer = std::make_unique<SpriteRenderer2D>();
    triggerRenderer->SetSpriteSheet(sheet);
    triggerRenderer->SetFrameIndex(0); 
    triggerObj->AddComponent(std::move(triggerRenderer));

    // 'true' explicitly maps this specifically to a trigger bounds!
    auto triggerC = std::make_unique<ColliderComponent>(glm::vec2(0.0f), glm::vec2(1.5f, 1.5f), true);
    triggerC->SetOnTriggerEnter([](ColliderComponent* self, ColliderComponent* other) {
        ENGINE_LOG("entity %s entered the area of %s", other->GetOwner()->GetName().c_str(), self->GetOwner()->GetName().c_str());
    });
    triggerC->SetOnTriggerExit([](ColliderComponent* self, ColliderComponent* other) {
        ENGINE_LOG("entity %s exited the area of %s", other->GetOwner()->GetName().c_str(), self->GetOwner()->GetName().c_str());
    });
    
    m_Physics.RegisterCollider(triggerC.get());
    triggerObj->AddComponent(std::move(triggerC));

    m_GameObjects.push_back(std::move(triggerObj));

    // Create secondary trigger zone
    ENGINE_LOG("OnEnter: creating second trigger object");
    auto triggerObj2 = std::make_unique<GameObject>("trigger_zone_2");
    m_Registry.Create(EntityCategory::Environment, "trigger_zone_2", "TestScene");
    triggerObj2->GetTransform().position = glm::vec2(0.0f, 3.0f);
    triggerObj2->GetTransform().size = glm::vec2(2.0f, 1.0f);

    auto triggerRenderer2 = std::make_unique<SpriteRenderer2D>();
    triggerRenderer2->SetSpriteSheet(sheet);
    triggerRenderer2->SetFrameIndex(0); 
    triggerObj2->AddComponent(std::move(triggerRenderer2));

    auto triggerC2 = std::make_unique<ColliderComponent>(glm::vec2(0.0f), glm::vec2(2.0f, 1.0f), true);
    triggerC2->SetOnTriggerEnter([](ColliderComponent* self, ColliderComponent* other) {
        ENGINE_LOG("entity %s entered the area of %s", other->GetOwner()->GetName().c_str(), self->GetOwner()->GetName().c_str());
    });
    triggerC2->SetOnTriggerExit([](ColliderComponent* self, ColliderComponent* other) {
        ENGINE_LOG("entity %s exited the area of %s", other->GetOwner()->GetName().c_str(), self->GetOwner()->GetName().c_str());
    });
    
    m_Physics.RegisterCollider(triggerC2.get());
    triggerObj2->AddComponent(std::move(triggerC2));

    m_GameObjects.push_back(std::move(triggerObj2));

    // Create second reference object (static, no animation)
    ENGINE_LOG("OnEnter: creating reference object");
    auto refObj = std::make_unique<GameObject>("static_ref");
    m_Registry.Create(EntityCategory::Environment, "static_ref", "TestScene");
    refObj->GetTransform().position = glm::vec2(3.0f, 0.0f);
    refObj->GetTransform().size = glm::vec2(0.5f, 0.5f);

    auto refSpriteRenderer = std::make_unique<SpriteRenderer2D>();
    refSpriteRenderer->SetSpriteSheet(sheet);
    refSpriteRenderer->SetFrameIndex(0);  // Static frame
    refObj->AddComponent(std::move(refSpriteRenderer));

    auto envCollider = std::make_unique<ColliderComponent>(glm::vec2(0.0f), glm::vec2(0.5f, 0.5f));
    m_Physics.RegisterCollider(envCollider.get());
    refObj->AddComponent(std::move(envCollider));

    m_GameObjects.push_back(std::move(refObj));
    ENGINE_LOG("OnEnter: created reference object at (3.0, 0.0)");


    // Set a reasonable view
    m_Camera.SetScale(1.0f);
    m_Camera.SetCameraPosition(0.0f, 0.0f);

    // std::cout << "TestScene: entered and created 1 GameObject with Animator\n";
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
    glm::vec2 moveDir(0.0f);
    glm::vec2 cameraDir(0.0f);

    if (m_Input)
    {
        // WASD for object movement
        if (m_Input->IsKeyDown(GLFW_KEY_W)) moveDir.y += 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_S)) moveDir.y -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_A)) moveDir.x -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_D)) moveDir.x += 1.0f;

        // Arrow keys for camera movement
        if (m_Input->IsKeyDown(GLFW_KEY_UP))    cameraDir.y += 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_DOWN))  cameraDir.y -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_LEFT))  cameraDir.x -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_RIGHT)) cameraDir.x += 1.0f;
    }

    // Apply input-driven movement to first GameObject (controllable sprite)
    if (m_GameObjects.size() > 0)
    {
        glm::vec2 oldPos = m_GameObjects[0]->GetTransform().position;
        m_GameObjects[0]->GetTransform().position += moveDir * m_MoveSpeed * deltatime;

        m_Physics.Update(deltatime);

        ColliderComponent* playerCollider = m_GameObjects[0]->GetComponent<ColliderComponent>();
        if (playerCollider && m_Physics.HasSolidCollision(playerCollider))
        {
            m_GameObjects[0]->GetTransform().position = oldPos;
        }
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
#endif
}
