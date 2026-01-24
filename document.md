# Project Overview & Usage

This document explains the project structure, runtime flow, and how to use the codebase for development and testing.

## High-level architecture

- `core/` — engine core: `Engine`, `Game` interface, `System` (GLFW), `Input`, `Timer`, and high-level wiring.
- `render/` — rendering code: `RenderManager`, `SpriteRendererClass`, and an older `OpenGLClass` wrapper (kept for now).
- `assets/` — `AssetManager` (data-only): loads `.tga`, `.ssheet`, `.anim`, `.objasset` and uploads `TextureGPU` to GL.
- `levels/` — scene and level definitions and scene manager.
- `objects/` — game objects and components (Animator, Sprite, generic Component base).
- `external/` — third-party libraries: `glad`, `glm` headers and sources.
- `test/` — place to add test harnesses like `testgame`/`testscene`.
- `bin/`, `obj/` — build outputs.

## Entry point & build

- `main.cpp` creates `Engine` and a `Game` (currently `SanityGame`).
- Build with the provided `makefile` (requires `pkg-config glfw3` and OpenGL dev libs):

```bash
make
./bin/framework
```

## Runtime flow

1. `Engine::Initialize(Game*)` constructs `System`, `Timer`, `Input`, `OpenGLClass` (legacy) and `Camera`, then calls `Game::OnInit()`.
2. `Engine::Run()` main loop:
   - `System::PollEvents()` → events pushed into `System::m_InputEvents`.
   - `Timer::Tick()` and `Input::ProcessEvents()` → updates input state.
   - `Game::OnInput()` → `Game::OnUpdate()` → `Game::OnRender()`.
   - Rendering is performed by render subsystems (`RenderManager` → `SpriteRendererClass`) or legacy `OpenGLClass`.
3. `Engine::Shutdown()` tears down systems and calls `Game::OnShutdown()`.

## Where responsibilities lie

- Gameplay: `Game`, `SceneManager`, `GameObject`, `Component` — no GL calls.
- Rendering orchestration: `RenderManager` builds draw lists and computes MVPs using `Camera`.
- GPU operations: `SpriteRendererClass` / `OpenGLClass` — manage GL state, shaders, VAO/VBO, texture binds.
- Assets: `AssetManager` loads and owns GPU textures and sprite metadata.

## Notes about `OpenGLClass`

- `OpenGLClass` is a legacy, low-level wrapper still present in `render/openglclass.*`. It is kept for compatibility with `SanityGame` and quick manual runs.
- Longer term plan: migrate consumers to `RenderManager` + `SpriteRendererClass` and remove `OpenGLClass`. For now we keep it and prioritize writing tests against higher-level APIs.

## Testing strategy (how to add tests)

- Prefer deterministic, fast tests that avoid real GL where possible.
  - Unit tests: inject or mock `System`, `Timer`, `Camera`, and `SpriteRendererClass`.
  - Integration harness: create `testgame`/`testscene` in `test/` that runs `Engine` for N frames and calls `Engine::Quit()`.
- When GL fidelity is required, run under headless X (Xvfb) or an EGL/OSMesa context in CI.
- Suggested first tests:
  1. `OnInput` behavior for `SanityGame` (WASD, arrows).
  2. `OnUpdate` moves player and camera as expected (use fake `Camera`).
 3. Lazy render initialization (renderer initializes once in `OnRender`).

## Practical next steps

1. Add `test/testgame.cpp` and `test/testscene.cpp` that implement minimal `Game`/`Scene` to exercise lifecycle.
2. Add small mocked `SpriteRenderer`/`Camera` test doubles to assert draw calls and camera updates without real GL.
3. Add a `make test` target that builds the harness; optionally prefix with `xvfb-run` when needed for headless runs.

## Quick references

- Entry point: [main.cpp](main.cpp#L1)
- Engine: [core/engine.cpp](core/engine.cpp#L1)
- System: [core/system.cpp](core/system.cpp#L1)
- Render manager: [render/rendermanager.cpp](render/rendermanager.cpp#L1)
- Sprite renderer: [render/spriterendererclass.cpp](render/spriterendererclass.cpp#L1)
- Assets: [assets/assetmanager.cpp](assets/assetmanager.cpp#L1)
- Sanity test: [sanitycheck.cpp](sanitycheck.cpp#L1)

---

If you want, I can now:
- scaffold the `test/` harness and a small set of test doubles, or
- implement a `make test` target and run the harness locally.

Tell me which and I'll proceed.
