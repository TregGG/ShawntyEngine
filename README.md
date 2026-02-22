# Framework Engine

A lightweight 2D game engine written in C++17 with clean separation between gameplay logic and rendering.

## Features

- **Component-based architecture** — Build game objects from reusable components
- **Scene management** — Organize game states with scene transitions
- **Sprite rendering & animation** — Modern render pipeline with sprite sheet and animation support
- **Asset system** — Pre-compiled asset loading for textures, sprite sheets, and animations
- **Input handling** — Keyboard input with GLFW integration
- **Separation of concerns** — Strict boundaries between gameplay code and OpenGL rendering

## Architecture

The engine is organized into distinct subsystems:

- **`core/`** — Engine loop, game interface, system management (GLFW), input, and timing
- **`render/`** — Rendering pipeline with `RenderManager` and `SpriteRendererClass`
- **`assets/`** — Asset loading and management for textures, sprite sheets, and animations
- **`levels/`** — Scene system and scene manager
- **`objects/`** — GameObject and component implementations
- **`external/`** — Third-party dependencies (GLAD, GLM)

### Key Design Principles

- **Gameplay code never calls OpenGL directly** — All rendering goes through the render subsystem
- **Component ownership** — GameObjects own components via `std::unique_ptr`, scenes own GameObjects
- **Data-driven assets** — Assets are loaded from pre-compiled formats (`.tga`, `.ssheet`, `.anim`, `.objasset`)
- **String-based asset IDs** — Type-safe asset references using `AssetID` and `ObjectID` types

## Getting Started

### Prerequisites

- C++17 compatible compiler (GCC, Clang)
- OpenGL development libraries
- GLFW3 (installed via package manager)
- pkg-config

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential libglfw3-dev libgl1-mesa-dev pkg-config
```

**Fedora:**
```bash
sudo dnf install gcc-c++ glfw-devel mesa-libGL-devel pkg-config
```

### Building

```bash
make
```

This compiles all source files and produces the executable at `bin/framework`.

### Running

```bash
./bin/framework
```

Currently runs `TestGame` with `TestScene` as a demonstration.

### Cleaning

```bash
make clean
```

Removes all object files and binaries.

## Creating a Game

1. **Implement the Game interface** by inheriting from `Game`:

```cpp
class MyGame : public Game
{
public:
    bool OnInit() override;
    void OnInput(const Input& input) override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnShutdown() override;
};
```

2. **Create a Scene** by inheriting from `Scene`:

```cpp
class MyScene : public Scene
{
public:
    void OnEnter() override;
    void OnExit() override;
    void Update(float deltaTime) override;
};
```

3. **Build GameObjects with Components**:

```cpp
auto gameObj = std::make_unique<GameObject>("Player");
gameObj->AddComponent(std::make_unique<SpriteRenderer2D>());
gameObj->AddComponent(std::make_unique<AnimatorComponent>());
```

4. **Wire it all together** in `main.cpp`:

```cpp
Engine engine;
MyGame game;

if (!engine.Initialize(&game))
    return -1;

engine.Run();
engine.Shutdown();
```

## Project Structure

```
framework/
├── main.cpp              # Entry point
├── makefile              # Build configuration
├── core/                 # Engine core systems
│   ├── engine.*          # Main engine loop
│   ├── game.h            # Game interface
│   ├── system.*          # GLFW window/context management
│   ├── input.*           # Input handling
│   └── timer.*           # Frame timing
├── render/               # Rendering subsystem
│   ├── rendermanager.*   # High-level render orchestration
│   ├── spriterendererclass.* # Sprite batch rendering
│   ├── camera.*          # 2D camera
│   └── openglclass.*     # Legacy OpenGL wrapper
├── assets/               # Asset loading and management
│   ├── assetmanager.*    # Asset loading API
│   └── assetdatastruct.h # Asset data structures
├── levels/               # Scene system
│   ├── scene.h           # Scene interface
│   └── scenemanager.*    # Scene lifecycle management
├── objects/              # Game object and component system
│   ├── gameobject.h      # GameObject base class
│   └── components/       # Built-in components
├── test/                 # Test games and scenes
└── external/             # Third-party libraries
    ├── glad/             # OpenGL loader
    └── glm/              # Math library
```

## Runtime Flow

1. **Initialization** — `Engine::Initialize()` creates subsystems (`System`, `Timer`, `Input`) and calls `Game::OnInit()`
2. **Main Loop** — `Engine::Run()` executes:
   - Poll GLFW events
   - Process input
   - Call `Game::OnInput()` → `Game::OnUpdate()` → `Game::OnRender()`
   - Render via `RenderManager` → `SpriteRendererClass`
3. **Shutdown** — `Engine::Shutdown()` tears down subsystems and calls `Game::OnShutdown()`

## Testing

The engine supports both unit and integration testing:

- **Unit tests** — Mock render systems and test gameplay logic in isolation
- **Integration tests** — Use test harnesses in `test/` that run `Engine` for N frames
- **Headless CI** — Run under `xvfb-run` or EGL/OSMesa for headless rendering

## Documentation

For detailed architecture notes and development guidelines, see [document.md](document.md).

For AI coding assistant guidance, see [.github/copilot-instructions.md](.github/copilot-instructions.md).

## License

[Your License Here]

## Contributing

Contributions are welcome! Please ensure code follows the existing patterns:
- Member variables use `m_` prefix
- No OpenGL calls in gameplay code
- Components must implement `Update(float deltaTime)`
- Use `std::unique_ptr` for ownership
