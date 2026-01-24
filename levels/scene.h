#ifndef SCENE_H_INCLUDED
#define SCENE_H_INCLUDED

#include <vector>
#include "../render/camera.h"
#include "../assets/assetdatastruct.h"
#include "../objects/gameobject.h"
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
     
    
    
    void SetInput(const Input& input) {m_Input=&input;};

    const std::vector<RenderableSprite>& GetRenderables() const {return m_Renderables;};
    Camera& GetCamera(){return m_Camera;};

protected:
    AssetManager* m_Assets = nullptr;
    const Input* m_Input = nullptr;
    std::vector<RenderableSprite> m_Renderables;
    Camera m_Camera;

};

#endif // SCENE_H_INCLUDED
