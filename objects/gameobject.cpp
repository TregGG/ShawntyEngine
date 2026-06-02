#include "gameobject.h"
#include "../levels/scene.h"

GameObject::GameObject(Scene* scene, const std::string& name, EntityCategory category)
    : m_Scene(scene)
{
    m_ID = scene->CreateEntity(category, name);
}
