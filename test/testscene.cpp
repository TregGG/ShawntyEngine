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
    gameObj->SetLayer(Layer::Player);

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

    auto playerCollider = std::make_unique<ColliderComponent>();
    playerCollider->SetAutoBounds(true);
    m_Physics.RegisterCollider(playerCollider.get());
    gameObj->AddComponent(std::move(playerCollider));

    ENGINE_LOG("OnEnter: creating RigidBodyComponent");
    auto playerRb = std::make_unique<RigidBodyComponent>();
    playerRb->SetType(BodyType::Dynamic);
    playerRb->SetDrag(10.0f); // Massive damping strictly mirroring tight top-down WASD snapping safely!
    playerRb->SetUseGravity(false);
    m_Physics.RegisterRigidBody(playerRb.get());
    gameObj->AddComponent(std::move(playerRb));

    m_GameObjects.push_back(std::move(gameObj));

    // Create trigger zone object
    ENGINE_LOG("OnEnter: creating trigger object");
    auto triggerObj = std::make_unique<GameObject>("trigger_zone");
    m_Registry.Create(EntityCategory::Environment, "trigger_zone", "TestScene");
    triggerObj->GetTransform().position = glm::vec2(-3.0f, 0.0f);
    triggerObj->GetTransform().size = glm::vec2(1.5f, 1.5f);
    triggerObj->SetLayer(Layer::UI); // Setting triggers explicitly to isolated UI Layer

    auto triggerRenderer = std::make_unique<SpriteRenderer2D>();
    triggerRenderer->SetSpriteSheet(sheet);
    triggerRenderer->SetFrameIndex(0); 
    triggerObj->AddComponent(std::move(triggerRenderer));

    // 'true' explicitly maps this specifically to a trigger bounds!
    auto triggerC = std::make_unique<ColliderComponent>(glm::vec2(0.0f), glm::vec2(1.0f), true);
    triggerC->SetAutoBounds(true);
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
    triggerObj2->SetLayer(Layer::UI);

    auto triggerRenderer2 = std::make_unique<SpriteRenderer2D>();
    triggerRenderer2->SetSpriteSheet(sheet);
    triggerRenderer2->SetFrameIndex(0); 
    triggerObj2->AddComponent(std::move(triggerRenderer2));

    auto triggerC2 = std::make_unique<ColliderComponent>(glm::vec2(0.0f), glm::vec2(1.0f), true);
    triggerC2->SetAutoBounds(true);
    // Explicitly dropping the Player layer from this trigger's mask!
    triggerC2->SetLayerMask(~(1 << static_cast<int>(Layer::Player)));
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
    refObj->SetLayer(Layer::Background); // Setting to explicitly filter layer for masking logic

    auto refSpriteRenderer = std::make_unique<SpriteRenderer2D>();
    refSpriteRenderer->SetSpriteSheet(sheet);
    refSpriteRenderer->SetFrameIndex(0);  // Static frame
    refObj->AddComponent(std::move(refSpriteRenderer));

    auto envCollider = std::make_unique<ColliderComponent>();
    envCollider->SetAutoBounds(true);
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
    glm::vec2 pushForce(0.0f);
    glm::vec2 cameraDir(0.0f);

    if (m_Input)
    {
        // WASD for object movement inherently tracking forces rather than static positioning natively!
        if (m_Input->IsKeyDown(GLFW_KEY_W)) pushForce.y += 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_S)) pushForce.y -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_A)) pushForce.x -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_D)) pushForce.x += 1.0f;

        // Arrow keys for camera movement
        if (m_Input->IsKeyDown(GLFW_KEY_UP))    cameraDir.y += 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_DOWN))  cameraDir.y -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_LEFT))  cameraDir.x -= 1.0f;
        if (m_Input->IsKeyDown(GLFW_KEY_RIGHT)) cameraDir.x += 1.0f;
    }

    // Apply input-driven movement to first GameObject (controllable sprite)
    if (m_GameObjects.size() > 0)
    {
        if (glm::length(pushForce) > 0.1f) {
            pushForce = glm::normalize(pushForce) * 150.0f; // High forces counteracting extreme Damping parameters logically.
        }

        RigidBodyComponent* rb = m_GameObjects[0]->GetComponent<RigidBodyComponent>();
        if (rb) {
            rb->AddForce(pushForce);
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
    
    // RAYCAST DEMONSTRATION
    // Shoots a laser directly horizontally/upwards dynamically mapped off user input axes or standard up vector
    m_TestLines.clear();
    if (m_GameObjects.size() > 0) {
        RaycastHit hit;
        glm::vec2 rayStart = m_GameObjects[0]->GetTransform().position;
        // Shoots a laser directly right explicitly as requested!
        glm::vec2 rayDir = glm::vec2(1.0f, 0.0f); 
        float rayLen = 50.0f;
        
        ColliderComponent* playerBounds = m_GameObjects[0]->GetComponent<ColliderComponent>();
        
        static std::string lastHit = "";
        
        // Explicity generating BitMask ignoring `Layer::UI` securely!
        uint32_t testingMask = ~(1 << static_cast<int>(Layer::UI)); 

        // Masking UI natively, meaning transparent Triggers get completely skipped across the bounds!
        if (RAYCAST_IGNORE_MASK(rayStart, rayDir, rayLen, hit, playerBounds, testingMask)) {
            // Target Hit = Red Vector projecting exactly onto hit wall!
            m_TestLines.push_back({rayStart, hit.point, glm::vec3(1.0f, 0.0f, 0.0f)});
            
            // Console properly outputting name of touched component owner
            std::string currentHit = hit.collider->GetOwner()->GetName();
            if (lastHit != currentHit) {
                ENGINE_LOG("Raycast just hit: %s", currentHit.c_str());
                lastHit = currentHit;
            }
        } else {
            lastHit = ""; // Clears the state safely when we hit empty space!
            // Target Missed = Continuous scaling Green Vector mappings!
            m_TestLines.push_back({rayStart, rayStart + rayDir * rayLen, glm::vec3(0.0f, 1.0f, 0.0f)});
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
#endif
}

void TestScene::BuildDebugLines(std::vector<DebugLine>& outDebugLines) const
{
#ifdef ENGINE_DEBUG
    outDebugLines = m_TestLines;
#endif
}
