#include "rendermanager.h"
#include "spriterendererclass.h"
#include "../objects/ui/uiobject.h"
#include<glad/glad.h>
#include "../levels/scene.h"
#define ENGINE_CLASS "RenderManager"
#include "../core/enginedebug.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>



bool RenderManager::Initialize()
{
    return m_SpriteRenderer.Initialize();
}

void RenderManager::Shutdown()
{
    m_SpriteRenderer.Shutdown();
}

void RenderManager::BindScene(Scene* scene)
{
    m_Scene = scene;
    m_Camera = scene ? &scene->GetCamera() : nullptr;
}

void RenderManager::OnScreenChange(int width, int height)
{
    if (!m_Camera || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;

    float aspect = static_cast<float>(width) / static_cast<float>(height);

    float baseWorldHeight = 10.0f; // engine-defined world height
    float zoom = m_Camera->GetScale(); // camera-controlled

    float viewHeight = baseWorldHeight * zoom;
    float viewWidth  = viewHeight * aspect;

    m_Camera->SetViewSize(viewWidth, viewHeight);
    ENGINE_LOG("OnScreenChange %dx%d -> view %.2fx%.2f", width, height, viewWidth, viewHeight);
}

void RenderManager::BeginFrame()
{
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_RenderQueue.clear();
    m_DebugQueue.clear();
    m_DebugLineQueue.clear();
}

void RenderManager::Render()
{
    if (!m_Scene || !m_Camera)
        return;

    CollectRenderables();
    CollectDebugRenderables();

    SubmitRenderables();
    SubmitDebugRenderables();
    
    RenderUI();
}

void RenderManager::CollectRenderables()
{
    std::vector<EntityID> renderables = m_Scene->registry.ViewTransformAndSprite();   
    const glm::mat4& vp = m_Camera->GetViewProjection();

    for (EntityID e : renderables)
    {
        const auto& transform = m_Scene->registry.GetComponent<TransformComponent>(e);
        const auto& sprite = m_Scene->registry.GetComponent<SpriteComponent2D>(e);

        if (!sprite.spriteSheet)
            continue;

        int frameIndex = sprite.frameIndex;

        // Animator overrides static frame if present
        if (m_Scene->registry.HasComponent<AnimatorComponent>(e)) {
            const auto& animator = m_Scene->registry.GetComponent<AnimatorComponent>(e);
            frameIndex = animator.GetFrameIndex();
        }

        if (frameIndex < 0 ||
            frameIndex >= static_cast<int>(sprite.spriteSheet->frames.size()))
            continue;

        // Note: For parents and children, the absolute world position should be computed.
        // For now, we assume transform.position is absolute world position, which should be
        // updated by the physics or transform system.
        glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                         glm::vec3(transform.position, 0.0f));

        model = glm::rotate(model,
                            transform.rotation,
                            glm::vec3(0, 0, 1));

        model = glm::scale(model,
                           glm::vec3(transform.size, 1.0f));

        glm::mat4 mvp = vp * model;

        m_RenderQueue.push_back({
            mvp,
            sprite.spriteSheet,
            frameIndex,
            sprite.layer
        });
    }
}


void RenderManager::SubmitRenderables()
{
    // Sort descending natively! Background (x) goes in first, UI (0) draws last specifically overwriting all frames on top continuously!
    std::sort(m_RenderQueue.begin(), m_RenderQueue.end(), [](const RenderEntry& a, const RenderEntry& b) {
        return static_cast<int>(a.layer) > static_cast<int>(b.layer);
    });

    for (const RenderEntry& entry : m_RenderQueue)
    {
        const SpriteFrame& frame =
            entry.sheet->frames[entry.frameIndex];

        m_SpriteRenderer.DrawSprite(
            *entry.sheet->texture,
            frame,
            entry.mvp
        );
    }
}

void RenderManager::CollectDebugRenderables()
{
#ifdef ENGINE_DEBUG
    std::vector<DebugRect> debugRects;
    m_Scene->BuildDebugRenderables(debugRects);

    const glm::mat4& vp = m_Camera->GetViewProjection();

    for (const DebugRect& r : debugRects)
    {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), r.position);
        model = glm::scale(model, glm::vec3(r.size, 1.0f));

        glm::mat4 mvp = vp * model;
        m_DebugQueue.push_back({mvp, r.color});
    }

    std::vector<DebugLine> debugLines;
    m_Scene->BuildDebugLines(debugLines);
    for (const DebugLine& l : debugLines)
    {
        glm::vec2 diff = l.end - l.start;
        float len = glm::length(diff);
        if (len < 0.0001f) continue;
        float angle = atan2(diff.y, diff.x);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(l.start, 0.0f));
        model = glm::rotate(model, angle, glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(len, 1.0f, 1.0f));

        glm::mat4 mvp = vp * model;
        m_DebugLineQueue.push_back({mvp, l.color});
    }
#endif
}

void RenderManager::SubmitDebugRenderables()
{
#ifdef ENGINE_DEBUG
    for (const DebugRenderEntry& entry : m_DebugQueue)
    {
        m_SpriteRenderer.DrawDebugRect(entry.mvp, entry.color);
    }
    for (const DebugRenderEntry& entry : m_DebugLineQueue)
    {
        m_SpriteRenderer.DrawDebugLine(entry.mvp, entry.color);
    }
#endif
}

void RenderManager::RenderUI()
{
    if (!m_Scene || m_ViewportWidth == 0 || m_ViewportHeight == 0) return;
    
    // Top-left is 0,0, bottom-right is width,height
    glm::mat4 projection = glm::ortho(0.0f, (float)m_ViewportWidth, (float)m_ViewportHeight, 0.0f, -1.0f, 1.0f);
    
    const auto& uiElements = m_Scene->registry.GetUIElements();
    for (const auto& ui : uiElements) {
        ui->Render(projection);
    }
}

void RenderManager::EndFrame()
{
    // future: debug draw, stats, batching flush
}


