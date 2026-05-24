# Asset Manager

The `AssetManager` is the engine's central warehouse for loading and storing external assets, such as textures, spritesheets, and animation clips.

---

## 1. Using Assets in Gameplay
You query assets from the `AssetManager` by their unique string identifiers. These assets are shared as raw, `const` pointers, meaning the GPU only loads texture coordinates once in memory.

### Applying a SpriteSheet:
To render an entity sprite, query the asset and assign it to a `SpriteComponent2D`:
```cpp
// 1. Fetch SpriteSheet pointer from the Scene's asset database
const SpriteSheetAsset* sheet = m_Assets->GetSpriteSheet("Player_Run");

// 2. Assign it to SpriteComponent2D
SpriteComponent2D ps;
ps.spriteSheet = sheet;
ps.frameIndex = 0;
ps.layer = Layer::Player;
scene->registry.AddComponent<SpriteComponent2D>(pID, ps);
```

### Playing Animations:
To animate a sprite sheet, assign the `AnimationSetAsset` using an `AnimatorComponent` in the registry:
```cpp
// 1. Fetch animation clip
const AnimationSetAsset* animData = m_Assets->GetAnimationSet("Player_Run");

// 2. Add AnimatorComponent to the entity
AnimatorComponent animator;
animator.Play(animData->GetClip("idle"), sheet, true);
scene->registry.AddComponent<AnimatorComponent>(pID, animator);
```

---

## 2. Memory Ownership (Const Pointers)
Pointers returned by `GetSpriteSheet()` and `GetAnimationSet()` are always `const`. The `AssetManager` retains sole ownership of the underlying assets. Entities only reference the address of the texture, meaning 100 spawned projectiles will consume no additional texture allocations.
