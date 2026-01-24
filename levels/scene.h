#ifndef SCENE_H_INCLUDED
#define SCENE_H_INCLUDED

#include <iostream>
#include <vector>
#include "../render/camera.h"
#include "../assets/assetdatastruct.h"
#include "../objects/gameobject.h"
#include "../objects/components/spriterenderer2d.h"
#include "../objects/components/animator.h"

class AssetManager;
class Input;


struct RenderableSprite
{
    Transform2D transform;

    // Provided by Animator
    const SpriteSheetAsset* spriteSheet = nullptr;
    int frameIndex = 0;
};


class Scene
{
public:
    virtual  ~Scene() = default;
    explicit Scene(AssetManager* assets)
        : m_Assets(assets)
    {}

    virtual void OnEnter() =0;
    virtual void OnExit()=0;
    virtual void Update(float deltatime)=0;

    virtual void BuildRenderables(std::vector<RenderableSprite>& outRenderables) const
    {
        // std::cout << "Scene: building renderables from " << m_GameObjects.size() << " GameObjects\n";
        outRenderables.clear();
        
        for (const auto& obj : m_GameObjects)
        {
            if (!obj || !obj->IsActive())
                continue;
        
            // Sprite renderer is required to render
            const SpriteRenderer2D* sprite =
                obj->GetComponent<SpriteRenderer2D>();
            if (!sprite || !sprite->GetSpriteSheet())
                continue;
        
            RenderableSprite r;
            r.transform   = obj->GetTransform();
            r.spriteSheet = sprite->GetSpriteSheet();
        
            // Animator overrides static frame if present
            if (const AnimatorComponent* animator =
                    obj->GetComponent<AnimatorComponent>())
            {
                r.frameIndex = animator->GetFrameIndex();
            }
            else
            {
                r.frameIndex = sprite->GetFrameIndex();
            }
        
            outRenderables.push_back(r);
        }
        std::cout << "Scene: built " << outRenderables.size() << " renderables\n";
    }
     
    
    
    void SetInput(const Input& input) {m_Input=&input;};

    // const std::vector<RenderableSprite>& GetRenderables() const {return m_Renderables;};
    Camera& GetCamera(){return m_Camera;};

protected:
    AssetManager* m_Assets = nullptr;
    const Input* m_Input = nullptr;
    std::vector<std::unique_ptr<GameObject>> m_GameObjects;
    // std::vector<RenderableSprite> m_Renderables;
    Camera m_Camera;

};

#endif // SCENE_H_INCLUDED
