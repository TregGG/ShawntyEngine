#include "testscene.h"
#include <iostream>
#include <glm/vec2.hpp>
#include "../assets/assetmanager.h"
#include "../core/input.h"
#include "../objects/gameobject.h"
#include "../objects/components/animator.h"
#include "../objects/components/spriterenderer2d.h"
#include <GLFW/glfw3.h>

void TestScene::OnEnter()
{
    if (!m_Assets)
    {
        std::cerr << "TestScene: no AssetManager provided\n";
        return;
    }

    std::cout << "TestScene::OnEnter: getting sprite sheet and animation set\n";
    // Get sprite sheet and animation set for "testobj"
    const SpriteSheetAsset* sheet = nullptr;
    const AnimationSetAsset* animSet = nullptr;
    try {
        sheet = m_Assets->GetSpriteSheet("testobj");
        std::cout << "TestScene::OnEnter: got sprite sheet\n";
        animSet = m_Assets->GetAnimationSet("testobj");
        std::cout << "TestScene::OnEnter: got animation set\n";
    } catch (const std::exception& e) {
        std::cerr << "TestScene: failed to load assets for 'testobj': " << e.what() << "\n";
        return;
    }

    std::cout << "TestScene::OnEnter: creating GameObject\n";
    // Create a GameObject and add AnimatorComponent and SpriteRenderer2D
    auto gameObj = std::make_unique<GameObject>("TestSprite");
    gameObj->GetTransform().position = glm::vec2(0.0f, 0.0f);
    gameObj->GetTransform().size = glm::vec2(2.0f, 2.0f);

    std::cout << "TestScene::OnEnter: creating AnimatorComponent\n";
    auto animator = std::make_unique<AnimatorComponent>();
    animator->BindAnimationSet(animSet, sheet);
    std::cout << "TestScene::OnEnter: animator bound, playing idle\n";
    animator->Play("idle", true);
    std::cout << "TestScene::OnEnter: animator playing\n";
    gameObj->AddComponent(std::move(animator));

    std::cout << "TestScene::OnEnter: creating SpriteRenderer2D\n";
    auto spriteRenderer = std::make_unique<SpriteRenderer2D>();
    spriteRenderer->SetSpriteSheet(sheet);
    spriteRenderer->SetFrameIndex(0);
    gameObj->AddComponent(std::move(spriteRenderer));

    m_GameObjects.push_back(std::move(gameObj));

    // Create second reference object (static, no animation)
    std::cout << "TestScene::OnEnter: creating reference object\n";
    auto refObj = std::make_unique<GameObject>("static_ref");
    refObj->GetTransform().position = glm::vec2(3.0f, 0.0f);
    refObj->GetTransform().size = glm::vec2(0.5f, 0.5f);

    auto refSpriteRenderer = std::make_unique<SpriteRenderer2D>();
    refSpriteRenderer->SetSpriteSheet(sheet);
    refSpriteRenderer->SetFrameIndex(0);  // Static frame
    refObj->AddComponent(std::move(refSpriteRenderer));

    m_GameObjects.push_back(std::move(refObj));
    std::cout << "TestScene::OnEnter: created reference object at (3.0, 0.0)\n";

    std::cout << "TestScene::OnEnter: building renderables\n";
    for (auto& obj : m_GameObjects)
    {
        if (obj && obj->IsActive())
        {
            // Query components for sprite data
            auto spriteRenderer = obj->GetComponent<SpriteRenderer2D>();
            auto animator = obj->GetComponent<AnimatorComponent>();
            if (spriteRenderer && spriteRenderer->GetSpriteSheet())
            {
                RenderableSprite r;
                r.transform = obj->GetTransform();
                r.spriteSheet = spriteRenderer->GetSpriteSheet();
                // Use animator's frame if available, otherwise sprite renderer's
                r.frameIndex = animator ? animator->GetFrameIndex() : spriteRenderer->GetFrameIndex();
                m_Renderables.push_back(r);
            }
        }
    }

    // Set a reasonable view
    m_Camera.SetViewSize(4.0f, 3.0f);
    m_Camera.SetCameraPosition(0.0f, 0.0f);

    std::cout << "TestScene: entered and created 1 GameObject with Animator\n";
}

void TestScene::SetInput(const Input& input)
{
    m_Input = &input;
}

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
        m_GameObjects[0]->GetTransform().position += moveDir * m_MoveSpeed * deltatime;
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

    // Rebuild renderables from GameObjects and their SpriteRenderer2D components
    m_Renderables.clear();
    for (auto& obj : m_GameObjects)
    {
        if (obj && obj->IsActive())
        {
            auto spriteRenderer = obj->GetComponent<SpriteRenderer2D>();
            auto animator = obj->GetComponent<AnimatorComponent>();
            if (spriteRenderer && spriteRenderer->GetSpriteSheet())
            {
                RenderableSprite r;
                r.transform = obj->GetTransform();
                r.spriteSheet = spriteRenderer->GetSpriteSheet();
                // Use animator's frame if available, otherwise sprite renderer's
                r.frameIndex = animator ? animator->GetFrameIndex() : spriteRenderer->GetFrameIndex();
                m_Renderables.push_back(r);
            }
        }
    }
}
