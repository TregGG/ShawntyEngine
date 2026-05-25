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

#include "../services/base/entityregistry/entityregistry.h"

struct DebugRect
{
    glm::vec3 position;
    glm::vec2 size;
    glm::vec3 color;
};

struct DebugLine
{
    glm::vec2 start;
    glm::vec2 end;
    glm::vec3 color;
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

    EntityID CreateEntity(EntityCategory category = EntityCategory::Environment, const std::string& name = "Entity") {
        return registry.Create(category, name, "Scene");
    }

    virtual void BuildDebugRenderables(std::vector<DebugRect>& /*outDebugRects*/) const {}
    virtual void BuildDebugLines(std::vector<DebugLine>& /*outDebugLines*/) const {}
    
    void SetInput(const Input& input) {m_Input=&input;};

    Camera& GetCamera(){return m_Camera;};

    EntityRegistryService registry; // The Database

protected:
    AssetManager* m_Assets = nullptr;
    const Input* m_Input = nullptr;
    std::vector<std::unique_ptr<GameObject>> m_GameObjects;
    Camera m_Camera;

};

#endif // SCENE_H_INCLUDED
