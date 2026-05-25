#pragma once
#include "../objects/gameobject.h"
#include "../services/base/entityregistry/entityregistry.h"

class Input;
struct SpriteSheetAsset;

class TestPlayer : public GameObject
{
private:
    EntityID m_WeaponID = 0;
    const Input* m_Input = nullptr;

public:
    TestPlayer(Scene* scene, const std::string& name, const SpriteSheetAsset* sheet);
    ~TestPlayer() override = default;

    EntityID GetWeaponID() const { return m_WeaponID; }

    void PassInput(const Input* input);
    void Update(float deltaTime);
};
