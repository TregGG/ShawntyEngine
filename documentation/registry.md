# Entity Registry Service

The `EntityRegistryService` is the central "Data Bucket" of the ECS architecture. Instead of heavy objects owning allocations, the registry maintains flat memory pools (`std::unordered_map`) for each component type.

---

## 1. Creating and Destroying Entities
Entities are represented as simple 64-bit unsigned integers (`EntityID`):
- **Lower 32-bits**: Slots vector index
- **Upper 32-bits**: Entity generation (protects against stale reference access)
- **ID 0** is reserved as the sentinel `Invalid` entity.

### Creation:
```cpp
// Created via the Scene context or directly on the registry
EntityID pID = scene->CreateEntity(EntityCategory::Player, "Player");
```

### Destruction:
Entities are destroyed by calling `Destroy`. Their slots are queued in a pending list and recycled in the next update cycle:
```cpp
scene->registry.Destroy(entityID);
```

---

## 2. Component Pools
Each component type is allocated inside its own continuous hash map pool inside `EntityRegistryService`:
```cpp
std::unordered_map<EntityID, TransformComponent> m_Transforms;
std::unordered_map<EntityID, SpriteComponent2D> m_Sprites;
std::unordered_map<EntityID, RelationshipComponent> m_Relationships;
std::unordered_map<EntityID, ColliderComponent> m_Colliders;
std::unordered_map<EntityID, RigidBodyComponent> m_RigidBodies;
std::unordered_map<EntityID, AnimatorComponent> m_Animators;
```

---

## 3. Template-based APIs
You can query or modify component data using clean, template-based interfaces resolved at compile-time:

- `AddComponent<T>(EntityID e, const T& comp)`
- `GetComponent<T>(EntityID e)`
- `HasComponent<T>(EntityID e)`
- `RemoveComponent<T>(EntityID e)`

```cpp
if (registry.HasComponent<TransformComponent>(entity)) {
    auto& trans = registry.GetComponent<TransformComponent>(entity);
    trans.position += glm::vec2(1.0f, 0.0f);
}
```

---

## 4. Query Views
Systems iterate procedurally over components of interest by querying specific views. Views return pre-filtered lists of matching entity IDs:

* **`ViewTransformAndSprite()`**: Returns entities possessing both a `TransformComponent` and a `SpriteComponent2D` (used by the `RenderManager`).
* **`ViewPhysicsObjects()`**: Returns entities possessing both a `TransformComponent` and a `ColliderComponent` (used by the `PhysicsSystem`).
* **`ViewAnimators()`**: Returns entities possessing an `AnimatorComponent`.
* **`ViewRelationships()`**: Returns entities with parent-child hierarchies defined via a `RelationshipComponent`.
