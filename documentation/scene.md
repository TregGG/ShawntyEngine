# Scene Management

A `Scene` represents a distinct state (like a Main Menu, options page, or level) in your game. It acts as the primary holder for the `EntityRegistryService` and schedules systems updates.

---

## 1. Creating a Scene
To implement a custom scene, subclass `Scene` and implement `OnEnter`, `OnExit`, and `Update` methods:

```cpp
#include "levels/scene.h"
#include "testplayer.h"

class MainLevel : public Scene {
public:
    MainLevel(AssetManager* assets) : Scene(assets) {}

    // Called when the scene is loaded
    void OnEnter() override {
        // 1. Initialize the registry
        registry.Init();
        m_Physics.Init();
        m_Physics.BindRegistry(&registry);

        const SpriteSheetAsset* sheet = m_Assets->GetSpriteSheet("PlayerSheet");

        // 2. Instantiate GameObjects (like TestPlayer)
        auto player = std::make_unique<TestPlayer>(this, "Player", sheet);
        m_GameObjects.push_back(std::move(player));
    }

    // Called every frame
    void Update(float deltatime) override {
        // Forward input and update wrappers
        if (!m_GameObjects.empty()) {
            if (auto* player = dynamic_cast<TestPlayer*>(m_GameObjects[0].get())) {
                player->PassInput(m_Input);
                player->Update(deltatime);
            }
        }

        // Run physics simulation
        m_Physics.Update(deltatime);

        // Process animator components
        for (EntityID e : registry.ViewAnimators()) {
            auto& animator = registry.GetComponent<AnimatorComponent>(e);
            if (animator.IsActive()) animator.Update(deltatime);
        }

        // Resolve transform hierarchy (rudimentary parent-child positioning)
        for (EntityID e : registry.ViewRelationships()) {
            const auto& rel = registry.GetComponent<RelationshipComponent>(e);
            if (rel.parent != 0) {
                auto& parentTrans = registry.GetComponent<TransformComponent>(rel.parent);
                auto& childTrans = registry.GetComponent<TransformComponent>(e);
                childTrans.position = parentTrans.position + childTrans.localPosition;
            }
        }
    }

    // Called when the scene is unloaded
    void OnExit() override {
        m_Physics.Shutdown();
        registry.Shutdown();
    }
};
```

---

## 2. Built-in Scene Properties
- **`registry`**: The database holding all active entities and their components.
- **`m_GameObjects`**: Pointers to factory wrappers (`GameObject` or subclasses like `TestPlayer`). These wrappers are automatically cleaned up when the scene is exited.
- **`m_Camera`**: View parameters (position, zoom, projection matrices) accessed via `GetCamera()`.
- **`m_Input`**: Pointer to the engine `Input` manager forwarding key/mouse press states to scenes.
