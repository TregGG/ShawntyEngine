#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <string_view>
#include "../../../core/entityid.h"
#include "../../service.h"
#include <unordered_map>
#include "../../../objects/components/components.h"
#include "../../../objects/components/collidercomponent.h"
#include "../../../objects/components/rigidbodycomponent.h"
#include "../../../objects/components/animator.h"

class EntityRegistryService : public Service
{
public:
    EntityRegistryService() = default;
    ~EntityRegistryService() override = default;

    void Init() override ;
    void Update(float dt) override;
    void Shutdown() override;

    // Create entity with category
    EntityID Create(EntityCategory category, std::string_view name, std::string_view registeredBy);

    void Destroy(EntityID entity);
    bool IsAlive(EntityID entity) const;

    // Category queries
    EntityCategory GetCategory(EntityID entity) const;

    const std::vector<std::uint32_t>& GetEntities(EntityCategory category) const;

    // --- Component Adders ---
    template<typename T> T& AddComponent(EntityID e, const T& comp = T{}) {
        if constexpr (std::is_same_v<T, TransformComponent>) { return m_Transforms[e] = comp; }
        else if constexpr (std::is_same_v<T, SpriteComponent2D>) { return m_Sprites[e] = comp; }
        else if constexpr (std::is_same_v<T, RelationshipComponent>) { return m_Relationships[e] = comp; }
        else if constexpr (std::is_same_v<T, ColliderComponent>) { return m_Colliders[e] = comp; }
        else if constexpr (std::is_same_v<T, RigidBodyComponent>) { return m_RigidBodies[e] = comp; }
        else if constexpr (std::is_same_v<T, AnimatorComponent>) { return m_Animators[e] = comp; }
        else { static_assert(sizeof(T) == 0, "Unsupported component type"); }
    }

    template<typename T> T& GetComponent(EntityID e) {
        if constexpr (std::is_same_v<T, TransformComponent>) { return m_Transforms[e]; }
        else if constexpr (std::is_same_v<T, SpriteComponent2D>) { return m_Sprites[e]; }
        else if constexpr (std::is_same_v<T, RelationshipComponent>) { return m_Relationships[e]; }
        else if constexpr (std::is_same_v<T, ColliderComponent>) { return m_Colliders[e]; }
        else if constexpr (std::is_same_v<T, RigidBodyComponent>) { return m_RigidBodies[e]; }
        else if constexpr (std::is_same_v<T, AnimatorComponent>) { return m_Animators[e]; }
        else { static_assert(sizeof(T) == 0, "Unsupported component type"); }
    }

    template<typename T> const T& GetComponent(EntityID e) const {
        if constexpr (std::is_same_v<T, TransformComponent>) { return m_Transforms.at(e); }
        else if constexpr (std::is_same_v<T, SpriteComponent2D>) { return m_Sprites.at(e); }
        else if constexpr (std::is_same_v<T, RelationshipComponent>) { return m_Relationships.at(e); }
        else if constexpr (std::is_same_v<T, ColliderComponent>) { return m_Colliders.at(e); }
        else if constexpr (std::is_same_v<T, RigidBodyComponent>) { return m_RigidBodies.at(e); }
        else if constexpr (std::is_same_v<T, AnimatorComponent>) { return m_Animators.at(e); }
        else { static_assert(sizeof(T) == 0, "Unsupported component type"); }
    }

    template<typename T> bool HasComponent(EntityID e) const {
        if constexpr (std::is_same_v<T, TransformComponent>) { return m_Transforms.find(e) != m_Transforms.end(); }
        else if constexpr (std::is_same_v<T, SpriteComponent2D>) { return m_Sprites.find(e) != m_Sprites.end(); }
        else if constexpr (std::is_same_v<T, RelationshipComponent>) { return m_Relationships.find(e) != m_Relationships.end(); }
        else if constexpr (std::is_same_v<T, ColliderComponent>) { return m_Colliders.find(e) != m_Colliders.end(); }
        else if constexpr (std::is_same_v<T, RigidBodyComponent>) { return m_RigidBodies.find(e) != m_RigidBodies.end(); }
        else if constexpr (std::is_same_v<T, AnimatorComponent>) { return m_Animators.find(e) != m_Animators.end(); }
        else { static_assert(sizeof(T) == 0, "Unsupported component type"); return false;}
    }

    template<typename T> void RemoveComponent(EntityID e) {
        if constexpr (std::is_same_v<T, TransformComponent>) { m_Transforms.erase(e); }
        else if constexpr (std::is_same_v<T, SpriteComponent2D>) { m_Sprites.erase(e); }
        else if constexpr (std::is_same_v<T, RelationshipComponent>) { m_Relationships.erase(e); }
        else if constexpr (std::is_same_v<T, ColliderComponent>) { m_Colliders.erase(e); }
        else if constexpr (std::is_same_v<T, RigidBodyComponent>) { m_RigidBodies.erase(e); }
        else if constexpr (std::is_same_v<T, AnimatorComponent>) { m_Animators.erase(e); }
        else { static_assert(sizeof(T) == 0, "Unsupported component type"); }
    }

    // Specific views for our engine
    std::vector<EntityID> ViewTransformAndSprite() const;
    std::vector<EntityID> ViewPhysicsObjects() const;
    std::vector<EntityID> ViewAnimators() const;
    std::vector<EntityID> ViewRelationships() const;

private:
    std::unordered_map<EntityID, TransformComponent> m_Transforms;
    std::unordered_map<EntityID, SpriteComponent2D> m_Sprites;
    std::unordered_map<EntityID, RelationshipComponent> m_Relationships;
    std::unordered_map<EntityID, ColliderComponent> m_Colliders;
    std::unordered_map<EntityID, RigidBodyComponent> m_RigidBodies;
    std::unordered_map<EntityID, AnimatorComponent> m_Animators;

private:

    struct Slot
    {
        std::uint32_t generation = 0;
        bool alive = false;
        EntityCategory category = EntityCategory::Environment;
        std::string name;
        std::string registeredBy;
    };

    std::vector<Slot> m_Slots;

    // Reuse destroyed slots
    std::vector<std::uint32_t> m_FreeList;

    // Deferred destruction
    std::vector<std::uint32_t> m_PendingDestroy;

    // Fast category access
    std::vector<std::vector<std::uint32_t>> m_CategoryBuckets;
};