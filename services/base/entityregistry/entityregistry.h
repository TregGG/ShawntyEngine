#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <string_view>
#include "../../../core/entityid.h"
#include "../../service.h"
#include <vector>
#include <deque>
#include "../../../objects/components/components.h"
#include "../../../objects/components/collidercomponent.h"
#include "../../../objects/components/rigidbodycomponent.h"
#include "../../../objects/components/animator.h"
#include "../../../objects/ui/uiobject.h"

template<typename T>
class ComponentPool {
private:
    std::deque<T> m_Data;
    std::vector<bool> m_Active;
    
public:
    T& Add(EntityID e, const T& comp) {
        uint32_t idx = EntityIndex(e);
        if (idx >= m_Data.size()) {
            m_Data.resize(idx + 1);
            m_Active.resize(idx + 1, false);
        }
        m_Data[idx] = comp;
        m_Active[idx] = true;
        return m_Data[idx];
    }
    
    T& Get(EntityID e) { return m_Data[EntityIndex(e)]; }
    const T& Get(EntityID e) const { return m_Data[EntityIndex(e)]; }
    
    bool Has(EntityID e) const { 
        uint32_t idx = EntityIndex(e);
        return idx < m_Active.size() && m_Active[idx]; 
    }
    
    void Remove(EntityID e) { 
        uint32_t idx = EntityIndex(e);
        if (idx < m_Active.size()) m_Active[idx] = false; 
    }
    
    void clear() {
        m_Data.clear();
        m_Active.clear();
    }
    
    const std::vector<bool>& GetActiveList() const { return m_Active; }
};

class EntityRegistryService : public Service
{
public:
    EntityRegistryService() = default;
    ~EntityRegistryService() override = default;

    void Init() override ;
    void Update(float dt) override;
    void Shutdown() override;

    void AddUIElement(std::unique_ptr<UIObject> element);
    const std::vector<std::unique_ptr<UIObject>>& GetUIElements() const;
    void ClearUIElements() { m_UIElements.clear(); }

    // Create entity with category
    EntityID Create(EntityCategory category, std::string_view name, std::string_view registeredBy);

    void Destroy(EntityID entity);
    bool IsAlive(EntityID entity) const;

    // Category queries
    EntityCategory GetCategory(EntityID entity) const;

    std::string_view GetName(EntityID entity) const {
        std::uint32_t index = EntityIndex(entity);
        if (index >= m_Slots.size()) return "Unknown";
        return m_Slots[index].name;
    }

    const std::vector<std::uint32_t>& GetEntities(EntityCategory category) const;

    // --- Component Adders ---
    template<typename T> T& AddComponent(EntityID e, const T& comp = T{}) {
        MarkViewsDirty();
        if constexpr (std::is_same_v<T, TransformComponent>) { return m_Transforms.Add(e, comp); }
        else if constexpr (std::is_same_v<T, SpriteComponent2D>) { return m_Sprites.Add(e, comp); }
        else if constexpr (std::is_same_v<T, RelationshipComponent>) { return m_Relationships.Add(e, comp); }
        else if constexpr (std::is_same_v<T, ColliderComponent>) { return m_Colliders.Add(e, comp); }
        else if constexpr (std::is_same_v<T, RigidBodyComponent>) { return m_RigidBodies.Add(e, comp); }
        else if constexpr (std::is_same_v<T, AnimatorComponent>) { return m_Animators.Add(e, comp); }
        else { static_assert(sizeof(T) == 0, "Unsupported component type"); }
    }

    template<typename T> T& GetComponent(EntityID e) {
        if constexpr (std::is_same_v<T, TransformComponent>) { return m_Transforms.Get(e); }
        else if constexpr (std::is_same_v<T, SpriteComponent2D>) { return m_Sprites.Get(e); }
        else if constexpr (std::is_same_v<T, RelationshipComponent>) { return m_Relationships.Get(e); }
        else if constexpr (std::is_same_v<T, ColliderComponent>) { return m_Colliders.Get(e); }
        else if constexpr (std::is_same_v<T, RigidBodyComponent>) { return m_RigidBodies.Get(e); }
        else if constexpr (std::is_same_v<T, AnimatorComponent>) { return m_Animators.Get(e); }
        else { static_assert(sizeof(T) == 0, "Unsupported component type"); }
    }

    template<typename T> const T& GetComponent(EntityID e) const {
        if constexpr (std::is_same_v<T, TransformComponent>) { return m_Transforms.Get(e); }
        else if constexpr (std::is_same_v<T, SpriteComponent2D>) { return m_Sprites.Get(e); }
        else if constexpr (std::is_same_v<T, RelationshipComponent>) { return m_Relationships.Get(e); }
        else if constexpr (std::is_same_v<T, ColliderComponent>) { return m_Colliders.Get(e); }
        else if constexpr (std::is_same_v<T, RigidBodyComponent>) { return m_RigidBodies.Get(e); }
        else if constexpr (std::is_same_v<T, AnimatorComponent>) { return m_Animators.Get(e); }
        else { static_assert(sizeof(T) == 0, "Unsupported component type"); }
    }

    template<typename T> bool HasComponent(EntityID e) const {
        if constexpr (std::is_same_v<T, TransformComponent>) { return m_Transforms.Has(e); }
        else if constexpr (std::is_same_v<T, SpriteComponent2D>) { return m_Sprites.Has(e); }
        else if constexpr (std::is_same_v<T, RelationshipComponent>) { return m_Relationships.Has(e); }
        else if constexpr (std::is_same_v<T, ColliderComponent>) { return m_Colliders.Has(e); }
        else if constexpr (std::is_same_v<T, RigidBodyComponent>) { return m_RigidBodies.Has(e); }
        else if constexpr (std::is_same_v<T, AnimatorComponent>) { return m_Animators.Has(e); }
        else { static_assert(sizeof(T) == 0, "Unsupported component type"); return false;}
    }

    template<typename T> void RemoveComponent(EntityID e) {
        MarkViewsDirty();
        if constexpr (std::is_same_v<T, TransformComponent>) { m_Transforms.Remove(e); }
        else if constexpr (std::is_same_v<T, SpriteComponent2D>) { m_Sprites.Remove(e); }
        else if constexpr (std::is_same_v<T, RelationshipComponent>) { m_Relationships.Remove(e); }
        else if constexpr (std::is_same_v<T, ColliderComponent>) { m_Colliders.Remove(e); }
        else if constexpr (std::is_same_v<T, RigidBodyComponent>) { m_RigidBodies.Remove(e); }
        else if constexpr (std::is_same_v<T, AnimatorComponent>) { m_Animators.Remove(e); }
        else { static_assert(sizeof(T) == 0, "Unsupported component type"); }
    }

    // Specific views for our engine
    const std::vector<EntityID>& ViewTransformAndSprite() const;
    const std::vector<EntityID>& ViewPhysicsObjects() const;
    const std::vector<EntityID>& ViewAnimators() const;
    const std::vector<EntityID>& ViewRelationships() const;

    void MarkViewsDirty() {
        m_ViewsDirty = true;
    }

private:
    void RebuildViews() const;
    ComponentPool<TransformComponent> m_Transforms;
    ComponentPool<SpriteComponent2D> m_Sprites;
    ComponentPool<RelationshipComponent> m_Relationships;
    ComponentPool<ColliderComponent> m_Colliders;
    ComponentPool<RigidBodyComponent> m_RigidBodies;
    ComponentPool<AnimatorComponent> m_Animators;

    mutable bool m_ViewsDirty = true;
    mutable std::vector<EntityID> m_CachedTransformSprite;
    mutable std::vector<EntityID> m_CachedPhysics;
    mutable std::vector<EntityID> m_CachedAnimators;
    mutable std::vector<EntityID> m_CachedRelationships;

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
    std::vector<std::unique_ptr<UIObject>> m_UIElements;

    // Reuse destroyed slots
    std::vector<std::uint32_t> m_FreeList;

    // Deferred destruction
    std::vector<std::uint32_t> m_PendingDestroy;

    // Fast category access
    std::vector<std::vector<std::uint32_t>> m_CategoryBuckets;
};