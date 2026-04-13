# GameObjects & Components

To support modular gameplay expansions dynamically natively scaling across infinite layers without inheriting rigid OOP logic classes deeply intertwining relationships, the engine heavily prefers `Entity-Component` layouts!

## Implementation Concepts

### 1. `GameObject` (The Shell)
The core physical presence object inside scenes. Its only fundamental variables inherently defined universally revolve exclusively around dynamic `Transform2D` matrices and active status flags!
```cpp
auto gameObj = std::make_unique<GameObject>("TestSprite");
gameObj->GetTransform().position = glm::vec2(0.0f, 0.0f);
```

### 2. `Component` (The Behavior)
A uniquely separated logic interface injected into objects directly utilizing `std::unique_ptr`s for pure structural memory lifetimes.
```cpp
class MyComponent : public Component {
public:
    void Update(float deltatime) override {
        // Manipulate m_Owner->GetTransform() logic locally
    }
};

// Injection
gameObj->AddComponent(std::make_unique<MyComponent>());
```

### Core Default Implementations
* **`AnimatorComponent`**: Native tracking linking explicit `AnimationSetAsset` frame limits driving state machines dynamically.
* **`SpriteRenderer2D`**: Passive visual tracking structure holding GPU IDs directly passed to external scenes!
* **`ColliderComponent`**: A dynamic AABB boundary tracking implementation calculating matrices completely independently natively avoiding rendering and physics tight-coupling! By calling `.SetAutoBounds(true)`, it natively scales the physical box directly to exactly mirror the visual graphic size of the `Transform2D` size values frame-by-frame!
