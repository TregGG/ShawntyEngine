#pragma once
#include "../objects/gameobject.h"
#include "../services/base/entityregistry/entityregistry.h"

class Input;
class AssetManager;
struct SpriteSheetAsset;

class TestPlayer : public GameObject
{
private:
    EntityID m_WeaponID = 0;
    const Input* m_Input = nullptr;

public:
    TestPlayer(Scene* scene, const std::string& name, const SpriteSheetAsset* sheet, AssetManager* assets = nullptr);
    ~TestPlayer() override = default;

    EntityID GetWeaponID() const { return m_WeaponID; }

    void PassInput(const Input* input);
    void Update(float deltaTime);
};
