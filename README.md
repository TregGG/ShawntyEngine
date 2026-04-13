# Framework Engine

A lightweight 2D game engine written in C++17 with a strictly decoupled architecture, meaning gameplay logic safely manages state without interacting directly with OpenGL rasterization pipelines.

## Getting Started

### Prerequisites

You need a C++17 compatible compiler seamlessly integrated with OpenGL/GLFW development libraries.
```bash
sudo apt-get install build-essential libglfw3-dev libgl1-mesa-dev pkg-config
```

### Build & Compiler Options
The engine compiles highly parallelized out of the box using recursive directory scans. We natively support varying console logging macros mapped dynamically to your build targets:

```bash
# Compiles everything seamlessly in multi-threaded mode (using `-j(nproc)` scaling under the hood).
make

# Execute the engine (Requires `bin/` folder)
./bin/framework
```

**Custom Macros:**
- `make BUILD=debug` (default) — Emits highly detailed engine logging identically to both standard console streams *and* a local `logs.txt` file setup (`ENGINE_LOG_BOTH`).
- `make BUILD=console` — Optimizes logging stream strictly forcing `ENGINE_LOG_CONSOLE` behavior.
- `make BUILD=file` — Discards standard std logs and writes silently tracking outputs to local file contexts.
- `make BUILD=release` — Completely trims the compilation removing ALL macro footprints for standard deployment.

**Tools:**
- `make clean` natively discards all cached `.o` intermediate references safely prior to new rebuilds.

## Creating a Game

Starting a game revolves fundamentally around overriding Engine instances and wrapping your behaviors logically within `Component` chunks.

1. **Implement the Root Engine Loop:**
```cpp
class MyGame : public Game
{
public:
    bool OnInit() override;
    void OnInput(const Input& input) override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;   // Offloads queries to specific Renderer
    void OnShutdown() override;
};
```

2. **Establish a Playing Field (`Scene`):**
```cpp
class MyScene : public Scene
{
public:
    void OnEnter() override;
    void OnExit() override;
    void Update(float deltatime) override;
};
```

3. **Deploy Physical Actors:**
```cpp
auto player = std::make_unique<GameObject>("Player");
player->AddComponent(std::make_unique<SpriteRenderer2D>());
player->AddComponent(std::make_unique<ColliderComponent>(glm::vec2(0.0f), glm::vec2(1.0f)));
```

4. **Integrate your Boot System** (`main.cpp`):
```cpp
Engine engine;
MyGame game;

if (!engine.Initialize(&game)) return -1;
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
├── services/             # Distinct background engine services
│   └── base/
│       ├── entityregistry/ # Registry for managing game entities dynamically
│       └── physics/        # Physics system parsing spatial partitioning and hits
├── test/                 # Test games and scenes
├── documentation/        # Detailed engine subsystem guides
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

## Logging

The engine has a built-in logger in `core/logger.*` with debug macros in `core/enginedebug.h`.

- In `ENGINE_RELEASE`, `ENGINE_LOG`, `ENGINE_WARN`, and `ENGINE_ERROR` are disabled.
- In `ENGINE_DEBUG`, logging is enabled and defaults to both console and file (`logs.txt`) via `ENGINE_LOG_BOTH` in `core/engineconfig.h`.
- `Engine::Initialize()` selects output mode (`Console`, `File`, or `Both`), and `Engine::Shutdown()` closes the logger.

Quick usage:

```cpp
#define ENGINE_CLASS "RenderManager"
#include "core/enginedebug.h"

ENGINE_LOG("Render queue size: %zu", m_RenderQueue.size());
ENGINE_WARN("Sprite sheet missing for object: %s", objectId.c_str());
ENGINE_ERROR("Shader compile failed");
```

Log format:

`[HH:MM:SS][LEVEL][Thread <id>][CLASS][Function:Line] message`

## Testing

The engine supports both unit and integration testing:

- **Unit tests** — Mock render systems and test gameplay logic in isolation
- **Integration tests** — Use test harnesses in `test/` that run `Engine` for N frames
- **Headless CI** — Run under `xvfb-run` or EGL/OSMesa for headless rendering

## Documentation

For deep structural breakdowns explaining our Quadtrees, slot registries, and renderer pipelines, see the [Documentation Module](documentation/):
- **[Physics Architecture](documentation/physics.md)**
- **[Rendering Pipeline](documentation/rendering.md)**
- **[Entity Registry System](documentation/registry.md)**
- **[Components & Layouts](documentation/components.md)**




## Contributing

Contributions are welcome! Please ensure code follows the existing patterns:
- Member variables use `m_` prefix
- No OpenGL calls in gameplay code
- Components must implement `Update(float deltaTime)`
- Use `std::unique_ptr` for ownership
