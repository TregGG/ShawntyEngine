# Physics System

The `PhysicsSystem` implements rigid body collision detection and response completely abstractly. It dynamically inherits from an internal `Service` framework, managing its spatial boundaries without ever directly associating with visual `GameObject` implementations.

## How it works

Instead of comparing every entity directly to every other entity ($O(n^2)$ complexity checks), the physics backend scales natively via a **Quadtree Spatial Partitioning** algorithm.

### 1. `ColliderComponent` Integration

A target relies on the `ColliderComponent` to calculate boundary layouts dynamically:
```cpp
class ColliderComponent : public Component {
    // Computes world-coordinates lazily against the runtime transform only when actively responding to query requests!
    AABB GetBounds() const;
};
```

Whenever a collider executes logic in the World Space, you inject it via:
```cpp
auto playerCollider = std::make_unique<ColliderComponent>(glm::vec2(0.0f), glm::vec2(2.0f, 2.0f));
m_Physics.RegisterCollider(playerCollider.get());
```

### 2. Recursive Partitioning
When `m_Physics.Update(deltatime)` runs prior to rendering frames, the core layout tears down and dynamically inserts active instances down partitioned localized quadrant leaves:
```cpp
void QuadtreeNode::Split() {
    float subWidth = (bounds.maxX - bounds.minX) / 2.0f;
    float subHeight = (bounds.maxY - bounds.minY) / 2.0f;
    // Dissects current boundaries identically into NW, NE, SW, SE zones
}
```

### 3. Collision Resolution
Queries recursively extract neighboring components occupying adjacent boundaries natively checking geometry overlaps.

To stop entities passing through each other, store boundaries prior to modifications and revert dynamically!
```cpp
// Apply Movement
glm::vec2 oldPos = obj->GetTransform().position;
obj->GetTransform().position += velocity * deltaTime;

// Refresh Engine Matrices
m_Physics.Update(deltaTime);

// Rollback rigid constraints!
if (m_Physics.HasCollision(myCollider)) {
    obj->GetTransform().position = oldPos;
}
```
