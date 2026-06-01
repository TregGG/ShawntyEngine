# Shawnty Engine

Welcome to the Shawnty Engine! This is a lightweight 2D game engine written in C++17. It is designed to be simple, fast, and easy to use. The engine keeps game logic and rendering completely separate, making your game code cleaner and less prone to graphics-related bugs.

## Getting Started

### Prerequisites

You need a C++17 compatible compiler and standard OpenGL/GLFW libraries installed.

**On Ubuntu/Debian:**
```bash
sudo apt-get install build-essential libglfw3-dev libgl1-mesa-dev pkg-config libfreetype6-dev python3-dev
```

**On Arch Linux (using pacman):**
```bash
sudo pacman -S base-devel glfw-x11 freetype2
```

### Building the Engine
You can compile the engine using `make`.

```bash
# Compile in parallel (fastest)
make

# Run the compiled engine
./bin/framework
```

**Build Options:**
- `make BUILD=debug` (default): Compiles with extra logging to help you find bugs. Logs are printed to the console and saved to `logs.txt`.
- `make BUILD=console`: Logs only to the console.
- `make BUILD=file`: Logs only to `logs.txt`.
- `make BUILD=release`: Compiles without any logging for maximum performance. Use this when you are ready to publish your game!

**Cleaning up:**
If you ever want to rebuild everything from scratch, run `make clean` first.

## Creating Your First Game

ShawntyEngine is **data-driven**. You don't need to write C++ code to design levels or gameplay logic. Instead, you design levels using JSON `.scene` files and write gameplay logic in Python.

1. **Design a Scene (JSON):**
Create a `level1.scene` file to define your entities and their components.
```json
{
    "version": 1,
    "scene": {
        "name": "Level 1",
        "entities": [
            {
                "name": "Player",
                "components": {
                    "transform": { "position": [10.0, 5.0] },
                    "sprite": { "layer": "Foreground" },
                    "rigidbody": { "bodyType": "Dynamic" },
                    "script": {
                        "path": "scripts/player.py",
                        "class": "PlayerController"
                    }
                }
            }
        ]
    }
}
```

2. **Write Game Logic (Python):**
Create `scripts/player.py`. Scripts can read input, apply physics, and respond to triggers.
```python
import shawnty

class PlayerController:
    def OnUpdate(self, entity, dt, input):
        if input.is_key_pressed(shawnty.KEY_SPACE):
            entity.get_rigidbody().apply_impulse(shawnty.Vec2(0, 15))
```

3. **Load the Scene in C++:**
In your main `Game` class, use `DataDrivenScene` to load everything automatically.
```cpp
#include "core/engine.h"
#include "levels/datadrivenscene.h"

class MyGame : public Game {
    DataDrivenScene* m_Scene;
public:
    bool OnInit() override {
        m_Scene = new DataDrivenScene(&m_AssetManager, "level1.scene");
        m_SceneManager.SetInitialScene(m_Scene);
        return true;
    }
    
    void OnUpdate(float deltaTime) override {
        m_SceneManager.Update(deltaTime);
    }
};

int main() {
    Engine engine;
    MyGame game;
    engine.Initialize(&game);
    engine.Run();
    return 0;
}
```

## Documentation

Want to learn more? Check out the detailed guides in the `documentation/` folder:

- **[Python Scripting](documentation/scripting_guide.md)**: How to write gameplay logic in Python.
- **[Multiplayer Architecture](documentation/multiplayer/architecture_physics_scenes.md)**: How client-server state sync works.
- **[Entity-Component System](documentation/components.md)**: How the underlying C++ components work.
- **[Physics & Collisions](documentation/physics.md)**: How bounding boxes, rigid bodies, and collisions work.
- **[Rendering Pipeline](documentation/rendering.md)**: How sprites and graphics are drawn.
- **[Entity Registry](documentation/registry.md)**: Grouping and finding game objects easily.
- **[Asset Manager](documentation/assetmanager.md)**: Loading textures and animations.

## Contributing

Contributions are welcome! Please ensure code follows the existing patterns:
- Member variables use `m_` prefix.
- No OpenGL calls in gameplay code (use the RenderManager).
- Use `std::unique_ptr` to manage object ownership safely.
