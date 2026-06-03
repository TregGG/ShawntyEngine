# ShawntyEngine — Python Scripting Guide

## Overview

ShawntyEngine supports Python scripting through an embedded Python interpreter powered by [pybind11](https://pybind11.readthedocs.io/). Scripts are attached to entities in scene JSON files and can:

- **Read and modify** entity component properties (position, velocity, size, etc.)
- **Respond to input** (keyboard and mouse)
- **Handle trigger events** (OnTriggerEnter / OnTriggerExit)
- **Run per-frame logic** (OnUpdate)

> **Key principle:** Scripts only modify *properties*. All physics simulation, rendering, and networking is handled by the C++ engine. A script sets `velocity = Vec2(5, 0)` — the engine does the rest.

---

## Quick Start

### 1. Create a Python script

```python
# test_compiled/scripts/my_script.py
import shawnty

class MyScript:
    speed = 5.0  # Can be overridden from scene JSON

    def OnStart(self, entity, input):
        print(f"Hello from {entity.get_name()}!")

    def OnUpdate(self, entity, dt, input):
        transform = entity.get_transform()
        if input.is_key_down(shawnty.KEY_D):
            pos = transform.position
            transform.position = shawnty.Vec2(pos.x + self.speed * dt, pos.y)
```

### 2. Attach it in your scene JSON

```json
{
    "editorId": "my_entity",
    "name": "MyEntity",
    "category": "Environment",
    "components": {
        "transform": {
            "position": [0.0, 0.0],
            "size": [1.0, 1.0]
        },
        "script": {
            "path": "test_compiled/scripts/my_script.py",
            "class": "MyScript",
            "properties": {
                "speed": 10.0
            }
        }
    }
}
```

### 3. Run the game

The engine automatically:
1. Loads the `.py` file
2. Creates an instance of `MyScript`
3. Sets `speed = 10.0` (from JSON properties)
4. Calls `OnStart()` once
5. Calls `OnUpdate()` every frame

---

## Script Lifecycle

| Method | Signature | When Called |
|--------|-----------|------------|
| `OnStart` | `(self, entity, input)` | Once, after the scene loads |
| `OnUpdate` | `(self, entity, dt, input)` | Every frame |
| `OnTriggerEnter` | `(self, entity, other, input)` | When a collision begins with a trigger |
| `OnTriggerExit` | `(self, entity, other, input)` | When a collision ends with a trigger |
| `OnDestroy` | `(self, entity)` | When the entity is destroyed or scene exits |

> All methods are **optional**. If your script doesn't define `OnUpdate`, nothing happens each frame. The engine checks with `hasattr()` before calling.

### Lifecycle Order Per Frame

```
1. Physics simulation (C++, 60Hz fixed timestep)
   └── Trigger callbacks fire here (OnTriggerEnter/Exit)
2. Script OnUpdate (Python, every frame)
3. Camera movement (C++)
4. Animator updates (C++)
5. Registry cleanup (C++)
```

---

## API Reference

### `shawnty.Vec2`

A 2D vector (wraps `glm::vec2`).

```python
import shawnty

v = shawnty.Vec2(3.0, 4.0)
v.x                    # 3.0
v.y                    # 4.0
v.length()             # 5.0
v.normalized()         # Vec2(0.6, 0.8)

# Arithmetic
a + b                  # Vec2 addition
a - b                  # Vec2 subtraction
a * 2.0                # Scalar multiply
2.0 * a                # Scalar multiply (reverse)
-a                     # Negate

# Static methods
shawnty.Vec2.zero()      # Vec2(0, 0)
shawnty.Vec2.one()       # Vec2(1, 1)
shawnty.Vec2.up()        # Vec2(0, 1)
shawnty.Vec2.right()     # Vec2(1, 0)
shawnty.Vec2.distance(a, b)
shawnty.Vec2.dot(a, b)
```

---

### `shawnty.Entity`

A handle to an engine entity. Passed as the first argument to all script methods.

```python
entity.get_name()       # "PlayerCharacter"
entity.get_id()         # 42 (runtime EntityID)
entity.is_alive()       # True/False

# Get component proxies (returns None if component doesn't exist)
transform = entity.get_transform()   # Transform or None
rb = entity.get_rigidbody()          # RigidBody or None
col = entity.get_collider()          # Collider or None
anim = entity.get_animator()         # Animator or None

entity.destroy()        # Destroys the entity
```

---

### `shawnty.Transform`

Read/write access to an entity's position, size, and rotation.

```python
transform = entity.get_transform()

# Position (world space)
transform.position                    # Vec2
transform.position = shawnty.Vec2(5, 3)

# Local position (relative to parent)
transform.local_position              # Vec2
transform.local_position = shawnty.Vec2(1, 0)

# Size
transform.size                        # Vec2
transform.size = shawnty.Vec2(2, 2)

# Rotation (radians)
transform.rotation                    # float
transform.rotation = 1.57

# World position (accounts for parent chain)
transform.get_world_position()        # Vec2
```

---

### `shawnty.RigidBody`

Controls physics properties. The C++ physics engine handles the actual simulation.

```python
rb = entity.get_rigidbody()

# Velocity (direct read/write)
rb.velocity                            # Vec2
rb.velocity = shawnty.Vec2(5, 0)       # Set directly

# Forces and impulses
rb.add_force(shawnty.Vec2(0, 100))     # Continuous force (per frame)
rb.apply_impulse(shawnty.Vec2(0, 10))  # Instant velocity change

# Properties
rb.mass = 2.0                # float
rb.drag = 0.5                # Linear damping
rb.gravity_scale = 2.0       # Multiplier for gravity
rb.use_gravity = True         # Enable/disable gravity
rb.elasticity = 0.8          # Bounciness (0 = none, 1 = perfect)
```

**Common patterns:**

```python
# Horizontal movement (preserve vertical velocity for gravity)
vel = rb.velocity
rb.velocity = shawnty.Vec2(move_speed, vel.y)

# Jump
if input.is_key_pressed(shawnty.KEY_SPACE):
    rb.apply_impulse(shawnty.Vec2(0, 18.0))

# Stop all movement
rb.velocity = shawnty.Vec2.zero()
```

---

### `shawnty.Collider`

Read/write access to collider settings.

```python
col = entity.get_collider()

col.is_trigger                # bool — is this a trigger zone?
col.is_trigger = True

col.layer_mask                # uint32 bitmask
col.layer_mask = 0xFFFFFFFF   # Collide with all layers
```

---

### `shawnty.Animator`

Controls sprite animation playback.

```python
anim = entity.get_animator()

anim.play("run")              # Play clip by name
anim.play("attack", loop=False)
anim.stop()
anim.has_animation("idle")    # True/False
anim.is_playing               # bool (read-only)
anim.set_speed(2.0)           # Playback speed multiplier
```

---

### `shawnty.Input`

Read-only access to keyboard and mouse state. Passed as the `input` argument to script methods.

```python
# Keyboard
input.is_key_down(shawnty.KEY_W)      # True while key is held
input.is_key_pressed(shawnty.KEY_W)   # True only on the frame key was pressed
input.is_key_released(shawnty.KEY_W)  # True only on the frame key was released

# Mouse
input.is_mouse_down(shawnty.MOUSE_LEFT)
input.is_mouse_pressed(shawnty.MOUSE_LEFT)
input.get_mouse_position()             # Vec2(x, y) in screen coords
```

---

### Key Constants

| Constant | Key |
|----------|-----|
| `KEY_W`, `KEY_A`, `KEY_S`, `KEY_D` | WASD movement |
| `KEY_Q`, `KEY_E`, `KEY_R`, `KEY_F` | Common actions |
| `KEY_SPACE` | Space bar |
| `KEY_ESCAPE` | Escape |
| `KEY_ENTER` | Enter/Return |
| `KEY_TAB` | Tab |
| `KEY_SHIFT` | Left Shift |
| `KEY_CTRL` | Left Control |
| `KEY_ALT` | Left Alt |
| `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT` | Arrow keys |
| `KEY_1` through `KEY_5` | Number keys |
| `MOUSE_LEFT`, `MOUSE_RIGHT`, `MOUSE_MIDDLE` | Mouse buttons |

---

## Editor Integration (Metadata / Enums)

You can expose script properties as dropdown menus in the ShawntyEngine editor Inspector by adding a special `# @export_enum` comment directly above the variable assignment in your script.

### General Options Dropdown
To create a dropdown with specific string options, list them separated by commas inside the parentheses:

```python
class MyScript:
    def OnStart(self, entity, input):
        # @export_enum(Red, Green, Blue) my_color
        if not hasattr(self, "my_color"):
            self.my_color = "Red"
```
*In the editor Inspector, `my_color` will now be a dropdown containing "Red", "Green", and "Blue".*

### Scene Selection Dropdown
To create a dropdown that automatically populates with a list of all existing `.scene` files in your project, use the special keyword `scenes`:

```python
class SceneTrigger:
    def OnStart(self, entity, input):
        # @export_enum(scenes) next_scene
        if not hasattr(self, "next_scene"):
            self.next_scene = "test_compiled/scenes/testscene2.scene"
```
*In the editor Inspector, `next_scene` will now be a dropdown listing all available scenes in your workspace, allowing you to easily link scenes without typing full paths.*

> **Syntax Note:** The syntax strictly requires the format `# @export_enum(options) variable_name` on a single line. The backend parser extracts this metadata without executing the Python code.

---

## Scene JSON Script Format

```json
"script": {
    "path": "test_compiled/scripts/my_script.py",
    "class": "MyScript",
    "properties": {
        "speed": 5.0,
        "jump_force": 18.0,
        "label": "\"Player One\""
    }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `path` | string | Path to the `.py` file (relative to working directory) |
| `class` | string | Name of the Python class to instantiate |
| `properties` | object | Key-value pairs set as attributes on the instance |

**Property values** are parsed as JSON. Numbers become `float`/`int`, booleans become `True`/`False`, strings need escaped quotes (`"\"hello\""`).

---

## Trigger Events

Trigger callbacks fire when entities with colliders overlap. The physics system calls `OnTriggerEnter` when overlap begins and `OnTriggerExit` when it ends.

**Both entities** receive the callback — if entity A (trigger) overlaps entity B (player), both A's script and B's script get `OnTriggerEnter`.

```python
class SceneTrigger:
    def OnTriggerEnter(self, entity, other, input):
        # 'entity' = this entity (the trigger)
        # 'other' = the entity that entered
        print(f"{other.get_name()} entered {entity.get_name()}")

    def OnTriggerExit(self, entity, other, input):
        print(f"{other.get_name()} left {entity.get_name()}")
```

```python
class PlayerController:
    def OnTriggerEnter(self, entity, other, input):
        # 'entity' = this entity (the player)
        # 'other' = the trigger zone
        print(f"I entered {other.get_name()}")
```

---

## Error Handling

If a Python script raises an exception, the engine:
1. **Catches** the error
2. **Logs** it with `[ERROR]` including the script name and method
3. **Continues running** — the game doesn't crash

Example error output:
```
[ERROR][ScriptEngine] [scripts/broken.py.MyScript] OnUpdate error: NameError: name 'undefined_var' is not defined
```

---

## Architecture

```
┌────────────────────────────────────────────────┐
│              ScriptEngine (C++)                │
│                                                │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐     │
│  │ Script 1 │  │ Script 2 │  │ Script 3 │     │
│  │ py::obj  │  │ py::obj  │  │ py::obj  │     │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘     │
│       │              │              │           │
│  EntityHandle   EntityHandle   EntityHandle     │
│       │              │              │           │
│  ┌────┴──────────────┴──────────────┴────┐     │
│  │       EntityRegistryService           │     │
│  │   TransformComponent pools            │     │
│  │   RigidBodyComponent pools            │     │
│  │   ColliderComponent pools             │     │
│  │   AnimatorComponent pools             │     │
│  │   ScriptComponent pools (data only)   │     │
│  └───────────────────────────────────────┘     │
└────────────────────────────────────────────────┘
```

### Proxy Pattern

Scripts never touch raw C++ pointers. Instead, they interact through **proxy objects** (`TransformProxy`, `RigidBodyProxy`, etc.) that hold a registry reference and entity ID. Every property access goes through the registry, which validates that the entity is alive before returning data.

```
Python script                C++ Proxy              C++ Registry
    │                            │                        │
    │  transform.position        │                        │
    │──────────────────────────>│                        │
    │                            │  HasComponent(entId)   │
    │                            │───────────────────────>│
    │                            │  GetComponent(entId)   │
    │                            │───────────────────────>│
    │                            │  return .position      │
    │  Vec2(3.0, 4.0)           │<───────────────────────│
    │<──────────────────────────│                        │
```

### File Structure

```
framework/
├── scripting/
│   ├── bindings.h           # Proxy struct definitions
│   ├── bindings.cpp          # PYBIND11_EMBEDDED_MODULE(shawnty)
│   ├── scriptengine.h        # ScriptEngine class (PIMPL, no pybind11 in header)
│   └── scriptengine.cpp      # ScriptEngine implementation
├── objects/components/
│   └── scriptcomponent.h     # Data-only struct (scriptPath, className, properties)
├── levels/
│   ├── datadrivenscene.h     # Owns ScriptEngine member
│   └── datadrivenscene.cpp   # Init/attach/update/trigger wiring
└── test_compiled/
    └── scripts/
        ├── scene_trigger.py      # Example: trigger zone callbacks
        └── player_controller.py  # Example: WASD movement + jumping
```

---

## Examples

### Example 1: Moving Platform

```python
import shawnty
import math

class MovingPlatform:
    amplitude = 3.0
    speed = 2.0

    def OnStart(self, entity, input):
        self.start_pos = entity.get_transform().position
        self.time = 0.0

    def OnUpdate(self, entity, dt, input):
        self.time += dt
        offset = math.sin(self.time * self.speed) * self.amplitude
        t = entity.get_transform()
        t.position = shawnty.Vec2(self.start_pos.x, self.start_pos.y + offset)
```

### Example 2: Collectible Item

```python
import shawnty

class Collectible:
    def OnTriggerEnter(self, entity, other, input):
        # Only react to players
        if "Player" in other.get_name():
            print(f"Collected by {other.get_name()}!")
            entity.destroy()
```

### Example 3: Camera Follow

```python
import shawnty

class CameraTarget:
    """Attach to the entity the camera should follow."""

    def OnUpdate(self, entity, dt, input):
        # Note: camera control requires C++ access
        # This script just prints the position for debugging
        pos = entity.get_transform().get_world_position()
        # A future version could expose camera control to scripts
```
