#pragma once

#include <vector>
#include <glm/glm.hpp>

class Scene;
class Camera;
#include "spriterendererclass.h"

#include "../core/entityid.h"
#include "../assets/assetmanager.h"

class RenderManager
{
public:
    bool Initialize();
    void Shutdown();

    // called only by Game::SetScene
    void BindScene(Scene* scene);

    // frame lifecycle
    void BeginFrame();
    void Render();
    void EndFrame();

    // window resize hook
    void OnScreenChange(int width, int height);

private:
    void CollectRenderables();
    void CollectDebugRenderables();
    void SubmitRenderables();
    void SubmitDebugRenderables();

private:
    Scene*  m_Scene  = nullptr;   // non-owning
    Camera* m_Camera = nullptr;   // non-owning

    SpriteRendererClass m_SpriteRenderer;

    struct RenderEntry
    {
        glm::mat4 mvp;
        const SpriteSheetAsset* sheet;
        int frameIndex;
        Layer layer;
    };
    
    struct DebugRenderEntry
    {
        glm::mat4 mvp;
        glm::vec3 color;
    };

    std::vector<RenderEntry> m_RenderQueue;
    std::vector<DebugRenderEntry> m_DebugQueue;
    std::vector<DebugRenderEntry> m_DebugLineQueue;

    int m_ViewportWidth  = 1;
    int m_ViewportHeight = 1;
};
