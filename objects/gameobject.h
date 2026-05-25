#pragma once
#include <string>
#include <glm/vec2.hpp>
#include <vector>
#include <memory>
#include "../core/entityid.h"


class Scene;

class GameObject
{
protected:
    EntityID m_ID;
    Scene* m_Scene;

public:
    virtual ~GameObject() = default;
    
    GameObject(Scene* scene, const std::string& name);

    EntityID GetID() const { return m_ID; }
    Scene* GetScene() const { return m_Scene; }
};
