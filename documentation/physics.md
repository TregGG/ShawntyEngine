# Physics & Collisions

The engine separates collision detection (bounding boxes and triggers) from physics simulation (movement, acceleration, and bouncing) using registry components.

---

## 1. Colliders (Hitboxes)
To make an entity interact with the physics system, attach a `ColliderComponent` using the registry. This defines an axis-aligned bounding box (AABB) around the entity.

```cpp
ColliderComponent pc;
pc.SetAutoBounds(true); // Automatically scales hitbox to match the sprite's size
scene->registry.AddComponent<ColliderComponent>(pID, pc);
```

### Triggers vs. Solid Colliders
By default, colliders are solid and displace other objects. Marking a collider as a trigger turns it into a sensor:
```cpp
pc.SetTrigger(true); // Objects pass right through it
```

### Handling Trigger Events
Triggers notify registered listeners on collision using OnTriggerEnter and OnTriggerExit events:
```cpp
pc.SetOnTriggerEnter([scene](EntityID self, EntityID other) {
    std::string_view selfName = scene->registry.GetName(self);
    std::string_view otherName = scene->registry.GetName(other);
    ENGINE_LOG("'%s' collided with '%s'", std::string(selfName).c_str(), std::string(otherName).c_str());
});
```

---

## 2. RigidBodies (Forces & Movement)
Adding a `RigidBodyComponent` to an entity enables physics updates (gravity, velocity, drag, elasticity) inside the `PhysicsSystem`.

```cpp
RigidBodyComponent prb;
prb.SetType(BodyType::Dynamic); // Dynamic (simulated), Static (walls/ground), or Kinematic (manual velocity)
prb.SetMass(1.0f);
prb.SetDrag(2.0f); // Damping coefficient slowing velocity
prb.SetUseGravity(true);
prb.SetGravityScale(2.0f);
scene->registry.AddComponent<RigidBodyComponent>(pID, prb);
```

### Applying Forces
Instead of manually updating positions, apply force vectors to the RigidBody component:
```cpp
if (m_Input->IsKeyDown(GLFW_KEY_D)) {
    auto& rb = scene->registry.GetComponent<RigidBodyComponent>(pID);
    rb.AddForce(glm::vec2(50.0f, 0.0f));
}
```

---

## 3. Raycasting & Shape Casting
The `PhysicsSystem` can test what entities intersect lines or shapes swept along a path.

### Raycasts
Shoots a ray of given length in a direction and returns a `RaycastHit` struct containing the point of collision and the `EntityID` of the object hit.
```cpp
RaycastHit hit;
glm::vec2 start = transform.position;
glm::vec2 direction(1.0f, 0.0f);
float length = 15.0f;

if (RAYCAST(start, direction, length, hit)) {
    std::string_view hitName = scene->registry.GetName(hit.entity);
    ENGINE_LOG("Ray hit object: %s", std::string(hitName).c_str());
}
```

### Shape Sweeps
Sweeps a Box or Circle shape centered along the path from `start` to `end` coordinates.
```cpp
RaycastHit hit;
uint32_t layerMask = 0xFFFFFFFF;

// Sweep a 1.0x1.0 box
if (BOX_CAST(start, end, glm::vec2(1.0f), hit, layerMask)) {
    // hit.point represents the surface point of contact
}

// Sweep a circle with a radius of 0.5
if (CIRCLE_CAST(start, end, 0.5f, hit, layerMask)) {
    ...
}
```
