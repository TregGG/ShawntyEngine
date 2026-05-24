# Entities & Components (ECS)

The engine uses a **Data-Oriented Entity-Component System (ECS)**. In this architecture, game objects are split from their data to maximize CPU cache locality, prevent memory leaks, and bypass virtual function call overheads.

---

## 1. `GameObject` (The Entity)
A `GameObject` is a lightweight container representing a single entity in the game world. It is a thin factory wrapper holding:
- A 64-bit `EntityID` (generated via the Scene's registry)
- A pointer to the parent `Scene`

When created, it registers itself inside the Scene's central `EntityRegistryService`.

```cpp
// Create a new GameObject wrapper
auto player = std::make_unique<GameObject>(scene, "Player");
EntityID pID = player->GetID();
```

---

## 2. Components (Pure Data)
Components in the engine are Plain Old Data (POD) structures. They do **not** inherit from a base `Component` class and do **not** store pointer references to their owner entity. Instead, they are stored in flat, contiguous memory pools inside the central `EntityRegistryService` and are associated with entities by their `EntityID`.

### Built-in Components:
* **`TransformComponent`**: Position (`position`), scale/size (`size`), local position (`localPosition` for nested hierarchies), and rotation (`rotation`).
* **`SpriteComponent2D`**: Sprite sheet asset pointer (`spriteSheet`), frame index (`frameIndex`), and rendering layer (`layer`).
* **`AnimatorComponent`**: Manages play state and updates active sprite sheet animation frames.
* **`ColliderComponent`**: Creates a bounding box (AABB) for spatial partitioning and collision tests.
* **`RigidBodyComponent`**: Mass, drag, velocity, elasticity, and gravity settings.
* **`RelationshipComponent`**: Defines parent-child relationships for nested coordinate hierarchies (e.g. Weapon tracking a Player).

---

## 3. Interacting with the Registry
You use the `registry` (found on your `Scene` class) to add, query, check, or remove component data.

### Adding Components:
```cpp
TransformComponent pt;
pt.position = glm::vec2(0.0f, 5.0f);
pt.size = glm::vec2(1.0f, 1.0f);
scene->registry.AddComponent<TransformComponent>(pID, pt);
```

### Retrieving & Mutating Components:
```cpp
// Check if an entity has a component
if (scene->registry.HasComponent<RigidBodyComponent>(pID)) {
    // Fetch and mutate the component
    auto& rb = scene->registry.GetComponent<RigidBodyComponent>(pID);
    rb.SetVelocity(glm::vec2(0.0f, 15.0f)); // Jump!
}
```

### Removing Components:
```cpp
scene->registry.RemoveComponent<ColliderComponent>(pID);
```

---

## 4. Custom Wrappers (e.g., `TestPlayer`)
To write custom logic for complex entities, create a wrapper class inheriting from `GameObject`. It handles registering its own component data upon construction and provides hooks for handling input and updates.

```cpp
// Header
class TestPlayer : public GameObject {
private:
    const Input* m_Input = nullptr;
public:
    TestPlayer(Scene* scene, const std::string& name, const SpriteSheetAsset* sheet);
    void PassInput(const Input* input) { m_Input = input; }
    void Update(float deltaTime);
};

// Source Constructor
TestPlayer::TestPlayer(Scene* scene, const std::string& name, const SpriteSheetAsset* sheet)
    : GameObject(scene, name)
{
    // Register Transform, Sprite, Collider, RigidBody...
    TransformComponent pt;
    pt.position = glm::vec2(0.0f, 5.0f);
    scene->registry.AddComponent<TransformComponent>(m_ID, pt);
    
    // Create children...
}

// Source Update
void TestPlayer::Update(float deltaTime) {
    if (m_Input && m_Input->IsKeyDown(GLFW_KEY_A)) {
        auto& rb = m_Scene->registry.GetComponent<RigidBodyComponent>(m_ID);
        rb.AddForce(glm::vec2(-50.0f, 0.0f));
    }
}
```
