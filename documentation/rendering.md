# Rendering Pipeline

The engine maintains a rigid division between Gameplay Code and actual API implementation bounds. No `GameObject` directly manages a Shader.

## Setup Structure
All graphics pipelines channel directly through the global `RenderManager`. It acts as the abstraction layer, iterating loaded contexts independently drawing natively executed graphics implementations like `SpriteRendererClass`.

### 1. Building Renderables
During a frame process, `RenderManager::Render()` dynamically executes `CollectRenderables()`. Instead of managing pointer logic, this calls `Scene::BuildRenderables(outRenderables);` which caches flattened output data. 
```cpp
// De-structures massive complex hierarchy graphs into raw render components
struct RenderableSprite {
    Transform2D transform;
    const SpriteSheetAsset* spriteSheet = nullptr;
    int frameIndex = 0;
};
```

### 2. Matrix Overlap
Once extracted, `.SubmitRenderables()` calculates complex MVP matrix scales applying orthogonal screen limits, scaling to precise `RenderEntry` blocks and executing `.DrawSprite()`!

## Debug Rendering (`ENGINE_DEBUG`)
If executing in development profiles, a secondary query hook executes identically parallel to the base sprite layer fetching explicitly invisible logic layouts (Like `ColliderComponents`).

```cpp
void TestScene::BuildDebugRenderables(std::vector<DebugRect>& outDebugRects) const {
#ifdef ENGINE_DEBUG
    outDebugRects.clear();
    for (const auto& obj : m_GameObjects) {
        if (auto col = objPtr->GetComponent<ColliderComponent>()) {
            // Parses specific logic boundaries translating visually to glowing visual neon layouts
            outDebugRects.push_back({pos, size, glm::vec3(0.0f, 1.0f, 0.0f)});
        }
    }
#endif
}
```
This safely binds data into a localized `m_DebugVAO`/`VBO` utilizing pure explicit GPU `GL_LINE_LOOP` operations. Without interfering with `Game` logic, the engine safely isolates raw openGL drawing.
