# Physics & Collisions

The engine handles physics efficiently by separating collision detection (hitboxes) from physics responses (movement and bouncing). 

## 1. Colliders (Hitboxes)

To make an object interact with the physics system, you must give it a `ColliderComponent`. This creates an invisible, un-rotated rectangle (AABB) around the object.

```cpp
// Create a collider box starting at offset (0,0) with a size of 10x10 pixels
auto collider = std::make_unique<ColliderComponent>(glm::vec2(0.0f), glm::vec2(10.0f));

// Automatically scale the hitbox to match the object's visual size
collider->SetAutoBounds(true); 

player->AddComponent(std::move(collider));
```

### Triggers vs. Solid Walls
By default, colliders are "solid." However, you can mark a collider as a **Trigger**. Triggers don't stop movement; instead, they act like sensors (e.g., a finish line, or a coin you can pick up).

```cpp
collider->SetTrigger(true); // Now objects can pass right through it
```

### Handling Collision Events
When two colliders overlap, the physics system generates an event. You can read these events to run custom logic, like taking damage when touching an enemy. 
The system detects two specific moments:
1. **OnTriggerEnter:** The exact frame two objects *start* touching.
2. **OnTriggerExit:** The exact frame two objects *stop* touching.

## 2. RigidBodies (Movement & Forces)

If you want an object to move smoothly using physics (gravity, friction, acceleration), add a `RigidBodyComponent`. 

The RigidBody handles the math for you. Instead of moving the object's position directly, you apply forces to the RigidBody, and the engine updates the position automatically.

```cpp
auto rb = std::make_unique<RigidBodyComponent>();

// Set the body type to dictate how it moves:
// - BodyType::Static (Default): Acts like a wall. Cannot be moved by forces or collisions.
// - BodyType::Kinematic: Can be moved manually by code (setting velocity), but ignores physics forces and bounce.
// - BodyType::Dynamic: Fully simulated. Pushed by forces, gravity, and bounces off other objects.
rb->SetType(BodyType::Dynamic); 

rb->SetMass(1.0f);
rb->SetDrag(10.0f);             // Drag acts like friction, slowing the object down over time

player->AddComponent(std::move(rb));
```

### Moving a RigidBody
To move the object, you push it by applying a force:

```cpp
// Inside your custom component's Update() method:
if (Input::IsKeyDown(KEY_RIGHT)) {
    // Push the player to the right
    m_Owner->GetComponent<RigidBodyComponent>()->AddForce(glm::vec2(50.0f, 0.0f));
}
```

### Automatic Push-Out (Displacement)
If a dynamic RigidBody hits a solid Collider (like a wall), the engine automatically calculates exactly how far it penetrated the wall and instantly pushes it back out. This prevents objects from clipping through walls, meaning you never have to write manual code to stop players from walking through terrain!

## 3. Raycasting & Shape Casting

The physics engine provides tools for checking what exists along a path. This is essential for line-of-sight checks, bullets, or thick laser beams.

### Raycasts
A raycast shoots an invisible line from a `start` point in a specific `direction` for a given `length`. It returns a `RaycastHit` struct containing the exact point and surface normal of the first object it hits.

```cpp
RaycastHit hit;
if (RAYCAST(startPos, direction, length, hit)) {
    // We hit something!
    ENGINE_LOG("Hit: %s", hit.collider->GetOwner()->GetName().c_str());
}
```

### Shape Casts (Sweeps)
Instead of a thin line, you can sweep an entire 2D shape (a Circle or a Box) from a `start` point to an `end` point. The shapes are strictly **centered around the line** you provide, meaning the center of the shape travels exactly from your `start` point to your `end` point.

```cpp
RaycastHit hit;
uint32_t layerMask = 0xFFFFFFFF; // Target all layers

// Sweeps a box of size (10x10) centered along the line from start to end
if (BOX_CAST(start, end, glm::vec2(10.0f, 10.0f), hit, layerMask)) {
    // hit.point is mathematically computed as the EXACT contact point on the surface!
}

// Sweeps a circle with a radius of 5.0 centered along the line from start to end
if (CIRCLE_CAST(start, end, 5.0f, hit, layerMask)) {
    // Perfect for testing thick projectiles or player sweeps!
}
```

## Under the Hood: Spatial Partitioning (Quadtree)
Checking if every object is touching every other object is very slow. To keep the engine running fast, the physics system uses a **Quadtree**. This divides the game world into a grid of smaller sectors. The engine only checks for collisions between objects that are in the same sector, significantly boosting performance.
