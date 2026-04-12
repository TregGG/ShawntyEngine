# Entity Registry

The `EntityRegistryService` offers massive decoupling by explicitly removing direct relational dependencies within active classes while keeping robust organizational memory across complex states.

## How it works

### Storage
The engine inherently utilizes continuous cached arrays `std::vector<Slot> m_Slots` scaling globally to handle high-count metadata mapping safely without risking direct memory segmentation.

When registering:
```cpp
uint32_t EntityRegistryService::Create(EntityCategory category, const std::string& name, const std::string& registeredBy)
{
    // The engine manages fragmented allocation by natively overriding 
    // obsolete/marked entities rather than restructuring the whole container
    if (!m_FreeIndices.empty()) {
        uint32_t idx = m_FreeIndices.back();
        // Overrides slot
        return idx;
    }
}
```

### Bucket Assignment
Every object is categorically tied directly to an internal structure bucket natively simplifying searches:
```cpp
enum class EntityCategory { Player, Enemy, Projectile, Environment };

// Querying specific metadata targets natively without expensive loops:
const auto& enemies = m_Registry.GetByCategory(EntityCategory::Enemy);
```

### Application Logic Logging
Anytime an instance ties natively to an active scene representation, the `EntityRegistryService` implements safe tracking logging details globally natively inside `#ifdef ENGINE_LOG_BOTH` footprints, capturing object names and specific instantiator states cleanly across logs!
