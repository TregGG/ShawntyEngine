#pragma once
#include "../levels/scene.h"
#include <vector>
#include <memory>
#include <glm/vec2.hpp>
#include "../services/base/entityregistry/entityregistry.h"
#include "../services/base/physics/physicssystem.h"

class GameObject;
class AssetManager;
class Input;

class TestScene : public Scene
{
public:
    explicit TestScene(AssetManager* assets)
        : Scene(assets)
    {}

    void OnEnter() override;
    void OnExit() override;
    void Update(float deltatime) override;
    // void SetInput(const Input& input) override;

private:

    // std::vector<std::unique_ptr<GameObject>> m_GameObjects;
    float m_MoveSpeed = 5.0f;
    EntityRegistryService m_Registry;
    PhysicsSystem m_Physics;
};
