# Rendering Pipeline

The engine separates your game's logic from the actual graphics code (OpenGL). This means you don't need to write custom shaders or OpenGL commands inside your GameObjects.

## How to Draw Something

To draw an image on the screen, all you need to do is attach a `SpriteRenderer2D` component to a GameObject and give it a Sprite Sheet.

```cpp
// 1. Create a GameObject
auto player = std::make_unique<GameObject>("Player");

// 2. Add a SpriteRenderer
auto spriteRenderer = std::make_unique<SpriteRenderer2D>();

// 3. Tell it which image to draw (loaded via AssetManager)
spriteRenderer->SetSpriteSheet(m_Assets->GetSpriteSheet("PlayerTexture"));

player->AddComponent(std::move(spriteRenderer));
```
That's it! The engine will automatically draw the image at the exact position and rotation defined by the `GameObject`'s `Transform2D`.

## How the Engine Renders (Under the Hood)

You don't need to understand this to make a game, but here is what happens behind the scenes every frame:

1. **Collection**: The `Scene` loops through every active `GameObject`. If an object has a `SpriteRenderer2D`, the scene grabs its position, image, and current animation frame, and packages this data into a simple list of `RenderableSprite` structures.
2. **Batching**: The `RenderManager` takes this list of simple structures. It calculates the final math (matrices) required to draw them relative to the camera.
3. **Execution**: The `SpriteRendererClass` takes the math and talks directly to your graphics card (via OpenGL) to draw the pixels on the screen.

## Debug Rendering

When building your game, it is often helpful to see invisible objects like physics hitboxes. If you compile the engine in `debug` mode (`make BUILD=debug`), the engine will automatically draw bright green outlines around all `ColliderComponent` hitboxes!

This is handled securely through a parallel debug-drawing system, meaning the debug lines will completely disappear when you compile your final game in `release` mode, costing zero performance.
