# Rendering Pipeline

The engine separates gameplay logic from rendering pipelines (OpenGL). Game systems configure rendering by registering rendering components into the registry, bypassing custom draw functions inside entity loops.

---

## 1. Registering renderable elements

To display a sprite on the screen, attach a `TransformComponent` and a `SpriteComponent2D` to your entity.

```cpp
// 1. Configure transform position & scale
TransformComponent pt;
pt.position = glm::vec2(0.0f, 0.0f);
pt.size = glm::vec2(1.0f, 1.0f);
scene->registry.AddComponent<TransformComponent>(pID, pt);

// 2. Configure sprite sheet and rendering properties
SpriteComponent2D ps;
ps.spriteSheet = m_Assets->GetSpriteSheet("PlayerTexture");
ps.frameIndex = 0;
ps.layer = Layer::Player; // Specifies layer sorting (UI, Player, Foreground, Background)
scene->registry.AddComponent<SpriteComponent2D>(pID, ps);
```

---

## 2. Rendering Step (Under the Hood)
Every frame, the `RenderManager` renders the scene using three clean phases:

1. **Collection**: The manager calls `registry.ViewTransformAndSprite()` to fetch all active renderable entities. It extracts `TransformComponent` and `SpriteComponent2D` parameters, resolves animation frame overrides from `AnimatorComponent` (if present), and constructs model-view-projection (MVP) matrices.
2. **Layer Sorting**: The render entries are sorted by their `layer` value:
   ```cpp
   std::sort(m_RenderQueue.begin(), m_RenderQueue.end(), [](const RenderEntry& a, const RenderEntry& b) {
       return static_cast<int>(a.layer) > static_cast<int>(b.layer);
   });
   ```
   *Background sprites are drawn first; UI sprites are drawn last on top of everything.*
3. **Execution**: The sorted batch is passed to the shader program and drawn via OpenGL calls.

---

## 3. Debug Rendering
When compiling in debug mode (`make BUILD=debug`), the `RenderManager` queries active colliders and cast sweeps to draw green wireframe boxes and lines on top of the scene. These calls are completely removed in release builds to eliminate overhead.
