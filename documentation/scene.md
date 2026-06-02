# Scene Management

A `Scene` represents a distinct state (like a Main Menu, options page, or level) in your game. It acts as the primary holder for the `EntityRegistryService` and schedules systems updates.

With the evolution of ShawntyEngine, hardcoded C++ scenes are no longer required. Instead, the engine utilizes a fully Data-Driven architecture that loads JSON-based `.scene` files at runtime.

---

## 1. Data-Driven Scenes (`DataDrivenScene`)

Instead of writing a C++ class for each level, you should use `DataDrivenScene`. It automatically parses JSON `.scene` files and populates the entity registry natively.

### Dynamic Loading
You can transition between scenes dynamically in Python scripts or C++ without needing to compile them:

```python
# In a Python script attached to an entity
import shawnty

def OnTriggerEnter(my_id, other_id):
    if shawnty.is_server():
        shawnty.change_scene("test_compiled/scenes/testscene2.scene")
```

When `change_scene` is called, the `SceneManager` transparently handles the cleanup of the old scene and spins up a `DataDrivenScene` populated by the new JSON file.

### Scene File Structure
Scenes are represented in human-readable JSON (often constructed automatically by the React-based visual editor).

```json
{
  "name": "Level 1",
  "entities": [
    {
      "id": 1,
      "tag": "PlayerSpawn",
      "transform": { "x": 0.0, "y": 10.0, "sx": 1.0, "sy": 1.0, "rot": 0.0 },
      "script": "scripts/player_spawner.py"
    },
    {
      "id": 2,
      "tag": "Ground",
      "transform": { "x": 0.0, "y": -5.0, "sx": 10.0, "sy": 1.0, "rot": 0.0 },
      "collider": { "type": 1, "w": 100.0, "h": 20.0, "layer": 1, "mask": 2 },
      "rigidbody": { "type": 0 }
    }
  ]
}
```

---

## 2. Live Sync (Hot Reloading)

`DataDrivenScene` implements a zero-overhead UDP socket listener that allows instant live-syncing with the Web Editor.

When you modify an entity, drag a platform, or update a script in the browser, the editor fires a lightweight UDP packet to the engine. The active `DataDrivenScene` receives this trigger in its non-blocking `Update` loop and invokes:

```cpp
m_SceneSerializer.LoadScene(m_SceneFilePath, &registry);
```

This seamlessly reconstructs the registry, re-attaches scripts, and resets the physics state in less than a millisecond without needing to restart the executable or recompile C++ code.

---

## 3. Creating a Custom Hardcoded Scene (Legacy / Advanced)

If you need advanced programmatic control (e.g., procedural generation) that bypasses the visual editor, you can still subclass `Scene` and manually instantiate entities.

```cpp
#include "levels/scene.h"
#include "objects/gameobject.h"

class ProceduralLevel : public Scene {
public:
    ProceduralLevel(AssetManager* assets) : Scene(assets) {}

    void OnEnter() override {
        registry.Init();
        m_Physics.Init();
        m_Physics.BindRegistry(&registry);

        // Manually spawn an entity using the C++ Wrapper
        auto player = std::make_unique<GameObject>(this, "Player");
        player->AddComponent<TransformComponent>(glm::vec2(0.0f, 10.0f));
        m_GameObjects.push_back(std::move(player));
    }

    void Update(float deltatime) override {
        // Run physics simulation
        m_Physics.Update(deltatime);

        // Process animations and hierarchies
        UpdateSystems(deltatime);
    }

    void OnExit() override {
        m_Physics.Shutdown();
        registry.Shutdown();
    }
};
```

---

## 4. Built-in Scene Properties
- **`registry`**: The `EntityRegistry` database holding all active entities and their components.
- **`m_Physics`**: The `PhysicsSystem` orchestrating spatial partitioning, rigid bodies, and collisions.
- **`m_GameObjects`**: Pointers to factory wrappers. These wrappers are automatically cleaned up when the scene is exited.
- **`m_Camera`**: View parameters (position, zoom, projection matrices) accessed via `GetCamera()`.
- **`m_Input`**: Pointer to the engine `Input` manager forwarding key/mouse press states to scenes.
