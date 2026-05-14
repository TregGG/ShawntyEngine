# GameObjects & Components

The engine uses an **Entity-Component System (ECS)**. This design makes it incredibly easy to add new features to your game without writing complex and hard-to-maintain inheritance trees.

## 1. `GameObject` (The Entity)
A `GameObject` is essentially an empty container that represents a single entity in your game world (like a player, a bullet, or a wall). By default, it only has a name, an active state, and a **Transform2D** (which holds its position, rotation, and scale).

```cpp
// Create a new GameObject named "Player"
auto player = std::make_unique<GameObject>("Player");

// Move the player to coordinates (100, 50)
player->GetTransform().position = glm::vec2(100.0f, 50.0f);
```

## 2. `Component` (The Behavior)
A `Component` is a reusable script or behavior that you attach to a `GameObject`. A `GameObject` does nothing on its own until you give it components!

For example, to make the player visible and give it collision, you add a rendering component and a physics component:
```cpp
player->AddComponent(std::make_unique<SpriteRenderer2D>());
player->AddComponent(std::make_unique<ColliderComponent>(glm::vec2(0.0f), glm::vec2(10.0f)));
```

### Writing Your Own Component
Creating custom logic is as simple as creating a new class that inherits from `Component` and overriding the `Update` method. 

Here is an example of a simple component that moves an object continuously to the right:

```cpp
class MoveRightComponent : public Component {
public:
    void Update(float deltatime) override {
        // m_Owner is a built-in pointer to the GameObject this component is attached to.
        // We use it to get the object's transform and modify its position.
        
        float speed = 50.0f;
        m_Owner->GetTransform().position.x += speed * deltatime;
    }
};

// Now you can attach it to any GameObject!
player->AddComponent(std::make_unique<MoveRightComponent>());
```

## Built-In Engine Components
The engine comes with several essential components right out of the box:

* **`SpriteRenderer2D`**: Draws an image (sprite) to the screen. 
* **`AnimatorComponent`**: Plays animations by rapidly switching between frames in a Sprite Sheet.
* **`ColliderComponent`**: Creates an invisible "hitbox" around the object so the physics system can detect when it bumps into other things.
* **`RigidBodyComponent`**: Adds physics properties like mass, velocity, and friction. Used alongside a `ColliderComponent` to make objects push each other and react to gravity or forces.
