# Asset Manager

The `AssetManager` is the engine's central warehouse for loading and storing external files, like images (textures, spritesheets) and animations.

By using the `AssetManager`, the engine guarantees that a file is only ever loaded from your hard drive **once**, even if a hundred different GameObjects are using it!

## How It Works

During the initialization of your game, the engine sets up the `AssetManager`. It looks in the `assets/` directory (or wherever your compiled asset root is) to load files into memory.

### Loading Data (Internal)
You generally don't load files directly in your gameplay code. The `AssetManager` reads configuration files or scans directories during boot-up to populate its internal dictionaries (`std::unordered_map`). 

It stores:
- **Textures**: Raw image data on the GPU.
- **SpriteSheets**: Textures divided into grids (frames) for animation.
- **Animation Sets**: Timing data that tells the engine how long to hold each frame of a SpriteSheet.

## Using Assets in Gameplay

When you are creating GameObjects in your `Scene`, you will ask the `AssetManager` for pointers to the assets you need by using their `ObjectID` or `AssetID` (which are usually just strings).

### Applying a SpriteSheet
To draw an object, fetch its SpriteSheet from the `AssetManager` and give it to a `SpriteRenderer2D`:

```cpp
auto player = std::make_unique<GameObject>("Player");
auto renderer = std::make_unique<SpriteRenderer2D>();

// Ask the AssetManager for the "Player_Run" spritesheet
const SpriteSheetAsset* sheet = m_Assets->GetSpriteSheet("Player_Run");
renderer->SetSpriteSheet(sheet);

player->AddComponent(std::move(renderer));
```

### Applying Animations
Similarly, if you want the sprite to animate automatically, you fetch its `AnimationSet` and provide it to an `AnimatorComponent`:

```cpp
auto animator = std::make_unique<AnimatorComponent>();

// Ask the AssetManager for the animation timings
const AnimationSetAsset* animData = m_Assets->GetAnimationSet("Player_Run");
animator->SetAnimationSet(animData);

player->AddComponent(std::move(animator));
```

### Why use Pointers?
Notice that `GetSpriteSheet()` and `GetAnimationSet()` return `const` pointers. This is extremely important! 

The `AssetManager` *owns* the memory for the image. By passing a pointer to the GameObject, the object is simply "looking" at the image. If you spawn 50 enemies, they all share the exact same pointer to a single image in RAM, saving massive amounts of memory.
