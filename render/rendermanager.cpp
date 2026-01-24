#include "rendermanager.h"
#include "spriterendererclass.h"
#include<glad/glad.h>
#include "../levels/scene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>



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

    float aspect = static_cast<float>(width) / static_cast<float>(height);

    float baseWorldHeight = 10.0f; // engine-defined world height
    float zoom = m_Camera->GetScale(); // camera-controlled

    float viewHeight = baseWorldHeight * zoom;
    float viewWidth  = viewHeight * aspect;

    m_Camera->SetViewSize(viewWidth, viewHeight);
    std::cout << "RenderManager: OnScreenChange to " << width << "x" << height
              << ", view size " << viewWidth << "x" << viewHeight << "\n";
}

void RenderManager::BeginFrame()
{
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_RenderQueue.clear();
}

void RenderManager::Render()
{
    if (!m_Scene || !m_Camera)
        return;

    CollectRenderables();
    SubmitRenderables();
}

void RenderManager::CollectRenderables()
{
    const auto& renderables = m_Scene->GetRenderables();
    const glm::mat4& vp = m_Camera->GetViewProjection();

    for (const RenderableSprite& r : renderables)
    {
        if (!r.spriteSheet)
            continue;

        if (r.frameIndex < 0 ||
            r.frameIndex >= static_cast<int>(r.spriteSheet->frames.size()))
            continue;

        // Build model matrix (hidden from gameplay)
        glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                         glm::vec3(r.transform.position, 0.0f));

        model = glm::rotate(model,
                            r.transform.rotation,
                            glm::vec3(0, 0, 1));

        model = glm::scale(model,
                           glm::vec3(r.transform.size, 1.0f));

        glm::mat4 mvp = vp * model;

        m_RenderQueue.push_back({
            mvp,
            r.spriteSheet,
            r.frameIndex
        });
    }
}


void RenderManager::SubmitRenderables()
{
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

void RenderManager::EndFrame()
{
    // future: debug draw, stats, batching flush
}


