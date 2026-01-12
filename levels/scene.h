#ifndef SCENE_H_INCLUDED
#define SCENE_H_INCLUDED

#include <vector>
#include "../render/camera.h"
#include "../assets/assetdatastruct.h"
#include "../objects/gameobject.h"
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

    virtual void OnEnter() =0;
    virtual void OnExit()=0;
    virtual void Update(float deltatime)=0;
//    virtual void Render() =0;

    const std::vector<RenderableSprite>& GetRenderables() const {return m_Renderables;};
    Camera& GetCamera(){return m_Camera;};

protected:
    std::vector<RenderableSprite> m_Renderables;
    Camera m_Camera;

};

#endif // SCENE_H_INCLUDED
