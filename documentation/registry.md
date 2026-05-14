# Entity Registry

The `EntityRegistryService` is a global system used to keep track of GameObjects and group them by category. 

When you have hundreds of objects in a scene, searching through all of them just to find the player or all active enemies can be very slow. The registry solves this by sorting objects into buckets automatically.

## How It Works

### Categories
Every object can be assigned an `EntityCategory`. 

```cpp
enum class EntityCategory { 
    Player, 
    Enemy, 
    Projectile, 
    Environment 
};
```

### Registering an Entity
When you create a new object that you want to be able to find later, you register it with the engine. The system gives you back a unique ID.

```cpp
// Register a new enemy
uint32_t enemyId = EntityRegistryService::Create(EntityCategory::Enemy, "Goblin", "SpawnerSystem");
```

### Finding Entities Quickly
The main benefit of the registry is quickly fetching all entities of a specific type. Instead of looping through every object in the game, you just ask the registry for the specific bucket you want.

```cpp
// Get a list of all active enemies instantly
const auto& enemies = m_Registry.GetByCategory(EntityCategory::Enemy);

for (const auto& enemy : enemies) {
    // Process enemy logic
}
```

## Memory Management
Behind the scenes, the registry stores data in flat, continuous arrays (`std::vector`). When an entity is destroyed, the registry doesn't delete the slot and shift everything around (which is slow). Instead, it marks the slot as "free". The next time you create an entity, the registry simply overwrites the old, free slot. This makes creating and destroying entities extremely fast!
