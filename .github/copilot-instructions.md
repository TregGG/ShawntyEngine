# Framework Engine - AI Coding Assistant Guide

## Architecture Overview

This is a 2D game engine with clean separation between gameplay and rendering layers.

**Core systems** (`core/`):
- `Engine` owns the main loop and coordinates `System` (GLFW), `Timer`, `Input`, and legacy `OpenGLClass`
- `Game` is the abstract interface - implement `OnInit`, `OnInput`, `OnUpdate`, `OnRender`, `OnShutdown`
- Games own three managers: `SceneManager`, `RenderManager`, `AssetManager`

**Component architecture** (`objects/`):
- `GameObject` has `Transform2D` and vector of `std::unique_ptr<Component>`
- Components are added via `AddComponent(std::make_unique<T>())` and retrieved via templated `GetComponent<T>()`
- Built-in components: `SpriteRenderer2D` (passive display), `AnimatorComponent` (drives frame updates)
- Components must implement `Update(float deltaTime)` and can access owner via `GetOwner()`

**Data flow**:
1. `AssetManager` loads pre-compiled assets from `test_compiled/` (`.tga`, `.ssheet`, `.anim`, `.objasset`)
2. Assets use string-based IDs (`AssetID`, `ObjectID`) - no implicit conversions
3. Rendering: `Scene` collects `RenderableSprite` → `RenderManager` builds MVP matrices → `SpriteRendererClass` submits to GPU
4. `AnimatorComponent` updates frame indices; `SpriteRenderer2D` holds current frame for rendering

## Critical Conventions

**Naming**:
- Member variables use `m_` prefix: `m_Scene`, `m_Camera`, `m_Running`
- Pointers marked as non-owning in comments when relevant
- Manager classes end in `Manager`: `AssetManager`, `SceneManager`, `RenderManager`

**Memory management**:
- `Engine` owns `System`, `Timer`, `Input` (raw pointers, manual delete)
- `Game` owns managers (stack-allocated members)
- `GameObject` owns components via `std::unique_ptr<Component>`
- Scenes own game objects via `std::vector<std::unique_ptr<GameObject>>`

**Asset system**:
- Assets are data-only - `AssetManager` loads and stores `TextureGPU`, `SpriteSheetAsset`, `AnimationSetAsset`
- Use `GetSpriteSheet(objectId)` and `GetAnimationSet(objectId)` for object-level queries
- `TextureGPU` contains OpenGL handle; `SpriteSheetAsset` holds non-owning `const TextureGPU*`

**Rendering separation**:
- NO OpenGL calls in gameplay code (`GameObject`, `Component`, `Scene`)
- `OpenGLClass` in `render/` is **legacy** - new code should use `RenderManager` + `SpriteRendererClass`
- `RenderManager` is the modern approach: collects renderables, computes MVPs, submits batches

## Build & Test Workflow

**Build**: `make` from project root produces `bin/framework`
- Requires `glfw3` (via `pkg-config`), OpenGL dev libs
- C++17, compiles all `*.cpp` in `core/`, `levels/`, `render/`, `assets/`, `objects/`, `test/`
- Object files go to `obj/`, binary to `bin/`

**Run**: `./bin/framework` (currently executes `TestGame` with `TestScene`)

**Clean**: `make clean` removes `obj/` and `bin/`

**Test strategy** (from `document.md`):
- Add test harnesses in `test/` (e.g., `testgame.cpp`, `testscene.cpp`)
- For headless CI: run under `xvfb-run` or EGL/OSMesa
- Prefer mocking render systems for unit tests; integration tests run `Engine` for N frames

## Key Files to Reference

- Entry point: [main.cpp](main.cpp) - creates `Engine` and `TestGame`
- Engine loop: [core/engine.cpp](core/engine.cpp) - `Initialize` → `Run` → `Shutdown`
- Game interface: [core/game.h](core/game.h) - base class for all games
- Component base: [objects/components/component.h](objects/components/component.h)
- Scene interface: [levels/scene.h](levels/scene.h)
- Asset data structures: [assets/assetdatastruct.h](assets/assetdatastruct.h)
- Comprehensive architecture notes: [document.md](document.md)

## When Adding Features

1. **New component**: Inherit from `Component`, implement `Update(float)`, use `GetOwner()` for GameObject access
2. **New game**: Inherit from `Game`, implement lifecycle methods, create and bind `Scene` in `OnInit()`
3. **New scene**: Inherit from `Scene`, implement `OnEnter`, `Update`, build `m_Renderables` vector
4. **Asset loading**: Add to `AssetManager` protected methods, follow existing patterns in `assetmanager.cpp`
5. **Rendering changes**: Work in `RenderManager`/`SpriteRendererClass`, avoid touching `OpenGLClass`
