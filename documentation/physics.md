# Physics System Architecture

The `PhysicsSystem` drives bounding box collisions natively through an explicitly decoupled `Service` architecture. It computes geometry limits completely isolated from Gameplay dependencies, operating transparently alongside visual abstractions.

## Core Component Interface

### `ColliderComponent` Features
The logic evaluates strictly against `ColliderComponent` structures. It evaluates the exact `AABB` dimensions exclusively on query calls, ensuring it doesn't incur constant framerate update costs:

```cpp
class ColliderComponent : public Component {
    // Re-evaluates world boundaries efficiently querying `GetTransform()` offsets solely when polled!
    AABB GetBounds() const;
    
    // Explicit behavioral flags dictate dynamic callbacks or strict blocks
    bool IsTrigger() const;
};
```

When building game states, you instantiate and explicitly inform the native engine loops:
```cpp
auto collider = std::make_unique<ColliderComponent>(glm::vec2(0.0f), glm::vec2(1.0f));
m_Physics.RegisterCollider(collider.get());
```

---

## 1. Spatial Partitioning Check
Instead of natively querying every bounding structure against each other ($O(n^2)$), the backend slices layouts into localized boundaries using a **Quadtree** system.

### Insertion
At the very beginning of the `m_Physics.Update(deltatime)` frame:
- The previous cache map completely destructs.
- `QuadtreeNode` bounds branch radially dividing active space into NW, NE, SW, and SE grids based on active object clusters.
- All valid instances dynamically shift down into the deepest local branch capable of housing them, severely lowering native overlap computations.

---

## 2. Event Dispatching & Trigger State Tracking
During the query phase, the exact overlap configurations hit a complex state-diffing system to parse behaviors natively correctly firing event queues!

A `CollisionEvent` object is constructed noting any active intersections, and pushed onto `m_Collisions`. Then, our `DispatchEvents()` runs a differential comparison:

1. **`OnTriggerEnter`** - Loop over `m_Collisions`. Any intersection mapping entirely missing from `m_PreviousCollisions` must be explicitly new! Run native callbacks on any involved `IsTrigger()` objects!
2. **`OnTriggerExit`** - Loop over `m_PreviousCollisions`. Any intersection parsing from the past frame but *missing* natively from the current frame indicates the entities broke contact! Safely resolve specific `.GetOnTriggerExit()` lambdas.

## 3. Continuous RigidBody Integration & Displacement

Previously, developers structured explicit boundary constraints internally by rolling back manual positional offsets. With the deployment of our `RigidBodyComponent`, bounding pushback is explicitly resolved mathematically during the physics step!

### `RigidBodyComponent`

The integration mathematically decouples positional changes from explicit `Update()` commands. Instead, objects dictate exactly how they interact with forces natively using physics constants:

```cpp
auto rb = std::make_unique<RigidBodyComponent>();
rb->SetType(BodyType::Dynamic);
rb->SetMass(1.0f);
rb->SetDrag(10.0f);       // Friction cleanly limits top speed!
rb->AddForce(pushVector); // Input logically maps to acceleration
```

### The Engine Loop
Inside `PhysicsSystem::Update(dt)`, the system inherently runs through completely interconnected systems natively:

1. **Euler Integration:** The engine iterates strictly pushing forces into `Velocity`, mapping acceleration and bounds natively across `dt`, effectively repositioning the shape completely decoupled from inputs!
2. **Manifold Mapping:** Rather than simply tracing generic boolean `Intersects`, overlaps precisely track spatial `depth` differences mapping the axis of penetration securely onto an explicit boundary `normal`.
3. **Displacement Matrix:** Resolves the final state iteratively calculating which structures overlap exactly. Using the manifold properties, it pushes dynamic components out of intersecting boundaries and securely clears out strictly sliding velocity scalars mapped directly across the wall structure, effectively neutralizing clipping natively without explicit state tracking rollbacks!
