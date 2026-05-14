# Scene Management

A `Scene` represents a distinct state or level in your game. For example, your Main Menu, Level 1, and Game Over screen would each be their own Scene.

## Creating a Scene

To create a new scene, create a class that inherits from `Scene` and implement its three core lifecycle methods: `OnEnter`, `OnExit`, and `Update`.

```cpp
#include "levels/scene.h"

class MainMenuScene : public Scene {
public:
    // Pass the AssetManager to the base Scene class
    MainMenuScene(AssetManager* assets) : Scene(assets) {}

    // Called once when the scene is loaded
    void OnEnter() override {
        // Create your GameObjects here
        auto background = std::make_unique<GameObject>("Background");
        background->GetTransform().position = glm::vec2(0.0f, 0.0f);
        
        // Add components...
        
        // Add the object to the scene's master list
        m_GameObjects.push_back(std::move(background));
    }

    // Called every frame
    void Update(float deltatime) override {
        // Call update on all GameObjects
        for(auto& obj : m_GameObjects) {
            if (obj && obj->IsActive()) {
                obj->Update(deltatime);
            }
        }
    }

    // Called once when the scene is unloaded
    void OnExit() override {
        // Clean up any custom memory here
        // Note: GameObjects in m_GameObjects are automatically cleaned up!
    }
};
```

## Scene Responsibilities

The `Scene` base class provides several important built-in features that you have access to:

### 1. `m_GameObjects` List
The scene owns the master list of all `GameObject`s. When the scene is destroyed, it automatically cleans up and deletes all objects in this list.

### 2. Rendering Collection
You do not need to write rendering code in your scene. The base `Scene` class provides a `BuildRenderables()` function. Every frame, the engine automatically calls this function. It scans your `m_GameObjects` list, finds everything with a `SpriteRenderer2D`, and sends it to the graphics system to be drawn.

### 3. `m_Camera`
Every scene has its own `Camera`. You can access it using `GetCamera()`. You can move the camera's position to follow the player around your level!

```cpp
GetCamera().SetPosition(playerPos);
```

### 4. Inputs and Assets
You have direct access to pointers for the `Input` system (to check if keys are pressed) and the `AssetManager` (`m_Assets`) to load new textures or objects during the level.
