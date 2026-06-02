#include "entityregistry.h"
#include "../../../objects/ui/uiobject.h"
#include <algorithm>

#define ENGINE_CLASS "EntityRegistryService"
#include "../../../core/enginedebug.h"

void EntityRegistryService::Init()
{
    m_CategoryBuckets.resize(static_cast<size_t>(EntityCategory::Count));

    // Reserve index 0 as invalid/sentinel slot
    Slot invalidSlot;
    invalidSlot.generation = 0;
    invalidSlot.alive = false;
    invalidSlot.category = EntityCategory::Environment;
    invalidSlot.name = "Invalid";
    invalidSlot.registeredBy = "System";
    m_Slots.push_back(invalidSlot);
}

EntityID EntityRegistryService::Create(EntityCategory category, std::string_view name, std::string_view registeredBy)
{
    std::uint32_t index;

    if (!m_FreeList.empty())
    {
        index = m_FreeList.back();
        m_FreeList.pop_back();

        Slot& slot = m_Slots[index];
        slot.alive = true;
        slot.category = category;
        slot.name = std::string(name);
        slot.registeredBy = std::string(registeredBy);

        // Add to category bucket
        m_CategoryBuckets[(size_t)category].push_back(index);

        ENGINE_LOG("Registered entity '%s' of category %d by '%s'", slot.name.c_str(), (int)category, slot.registeredBy.c_str());

        return MakeEntityID(index, slot.generation);
    }
    else
    {
        index = static_cast<std::uint32_t>(m_Slots.size());

        Slot slot;
        slot.generation = 0;
        slot.alive = true;
        slot.category = category;
        slot.name = std::string(name);
        slot.registeredBy = std::string(registeredBy);

        m_Slots.push_back(slot);

        // Add to category bucket
        m_CategoryBuckets[(size_t)category].push_back(index);

        ENGINE_LOG("Registered entity '%s' of category %d by '%s'", slot.name.c_str(), (int)category, slot.registeredBy.c_str());

        return MakeEntityID(index, slot.generation);
    }
}

void EntityRegistryService::Destroy(EntityID entity)
{
    std::uint32_t index = EntityIndex(entity);
    std::uint32_t generation = EntityGeneration(entity);

    if (index >= m_Slots.size())
        return;

    Slot& slot = m_Slots[index];

    if (!slot.alive || slot.generation != generation)
        return;

    // Defer destruction
    m_PendingDestroy.push_back(index);
}

bool EntityRegistryService::IsAlive(EntityID entity) const
{
    std::uint32_t index = EntityIndex(entity);
    std::uint32_t generation = EntityGeneration(entity);

    if (index >= m_Slots.size())
        return false;

    const Slot& slot = m_Slots[index];

    return slot.alive && slot.generation == generation;
}

EntityCategory EntityRegistryService::GetCategory(EntityID entity) const
{
    std::uint32_t index = EntityIndex(entity);

    if (index >= m_Slots.size())
        return EntityCategory::Environment;

    return m_Slots[index].category;
}

const std::vector<std::uint32_t>&
EntityRegistryService::GetEntities(EntityCategory category) const
{
    return m_CategoryBuckets[(size_t)category];
}

void EntityRegistryService::Update(float dt)
{
    for (std::uint32_t index : m_PendingDestroy)
    {
        Slot& slot = m_Slots[index];

        // Remove from category bucket
        auto& bucket = m_CategoryBuckets[(size_t)slot.category];

        for (size_t i = 0; i < bucket.size(); ++i)
        {
            if (bucket[i] == index)
            {
                bucket[i] = bucket.back();
                bucket.pop_back();
                break;
            }
        }

        slot.alive = false;
        slot.generation++; // invalidate old IDs
    }
    
    // Remove destroyed entities
    for(auto e : m_PendingDestroy)
    {
        RemoveComponent<TransformComponent>(e);
        RemoveComponent<SpriteComponent2D>(e);
        RemoveComponent<RelationshipComponent>(e);
        RemoveComponent<ColliderComponent>(e);
        RemoveComponent<RigidBodyComponent>(e);
        RemoveComponent<AnimatorComponent>(e);
        RemoveComponent<ScriptComponent>(e);

        // Clean up editor ID mapping
        if (e < m_Slots.size() && !m_Slots[e].editorId.empty()) {
            m_EditorIdMap.erase(m_Slots[e].editorId);
            m_Slots[e].editorId.clear();
        }

        m_FreeList.push_back(e);
    }
    m_PendingDestroy.clear();
    
    for (auto& ui : m_UIElements) {
        ui->Update(dt);
    }
}

void EntityRegistryService::Shutdown()
{
    m_UIElements.clear();
    m_Transforms.clear();
    m_Sprites.clear();
    m_Relationships.clear();
    m_Colliders.clear();
    m_RigidBodies.clear();
    m_Animators.clear();
    m_Scripts.clear();
    m_Slots.clear();
    m_FreeList.clear();
    m_PendingDestroy.clear();
    m_CategoryBuckets.clear();
    m_EditorIdMap.clear();
}

// --- Editor ID System ---

void EntityRegistryService::SetEditorId(EntityID entity, const std::string& editorId) {
    std::uint32_t index = EntityIndex(entity);
    if (index >= m_Slots.size()) return;
    
    // Remove old mapping if exists
    if (!m_Slots[index].editorId.empty()) {
        m_EditorIdMap.erase(m_Slots[index].editorId);
    }
    
    m_Slots[index].editorId = editorId;
    if (!editorId.empty()) {
        m_EditorIdMap[editorId] = entity;
    }
}

static const std::string s_EmptyString;

const std::string& EntityRegistryService::GetEditorId(EntityID entity) const {
    std::uint32_t index = EntityIndex(entity);
    if (index >= m_Slots.size()) return s_EmptyString;
    return m_Slots[index].editorId;
}

EntityID EntityRegistryService::FindByEditorId(const std::string& editorId) const {
    auto it = m_EditorIdMap.find(editorId);
    if (it != m_EditorIdMap.end()) return it->second;
    return 0; // Invalid entity
}

void EntityRegistryService::AddUIElement(std::unique_ptr<UIObject> element) {
    m_UIElements.push_back(std::move(element));
}

const std::vector<std::unique_ptr<UIObject>>& EntityRegistryService::GetUIElements() const {
    return m_UIElements;
}

static UIObject* RecursiveFind(UIObject* root, const std::string& name, const EntityRegistryService* registry) {
    if (registry->GetName(root->GetID()) == name) return root;
    for (const auto& child : root->GetChildren()) {
        UIObject* found = RecursiveFind(child.get(), name, registry);
        if (found) return found;
    }
    return nullptr;
}

UIObject* EntityRegistryService::FindUIElementRecursive(const std::string& name) const {
    for (const auto& el : m_UIElements) {
        UIObject* found = RecursiveFind(el.get(), name, this);
        if (found) return found;
    }
    return nullptr;
}

void EntityRegistryService::RebuildViews() const
{
    if (!m_ViewsDirty) return;

    m_CachedTransformSprite.clear();
    m_CachedPhysics.clear();
    m_CachedAnimators.clear();
    m_CachedRelationships.clear();

    const auto& activeTransforms = m_Transforms.GetActiveList();
    const auto& activeSprites = m_Sprites.GetActiveList();
    const auto& activeColliders = m_Colliders.GetActiveList();
    const auto& activeAnimators = m_Animators.GetActiveList();
    const auto& activeRels = m_Relationships.GetActiveList();

    for (size_t i = 0; i < m_Slots.size(); ++i) {
        if (!m_Slots[i].alive) continue;
        
        EntityID e = MakeEntityID(i, m_Slots[i].generation);
        bool hasTrans = i < activeTransforms.size() && activeTransforms[i];

        if (hasTrans && i < activeSprites.size() && activeSprites[i]) {
            m_CachedTransformSprite.push_back(e);
        }
        if (hasTrans && i < activeColliders.size() && activeColliders[i]) {
            m_CachedPhysics.push_back(e);
        }
        if (i < activeAnimators.size() && activeAnimators[i]) {
            m_CachedAnimators.push_back(e);
        }
        if (i < activeRels.size() && activeRels[i]) {
            m_CachedRelationships.push_back(e);
        }
    }

    m_ViewsDirty = false;
}

const std::vector<EntityID>& EntityRegistryService::ViewTransformAndSprite() const
{
    RebuildViews();
    return m_CachedTransformSprite;
}

const std::vector<EntityID>& EntityRegistryService::ViewPhysicsObjects() const
{
    RebuildViews();
    return m_CachedPhysics;
}

const std::vector<EntityID>& EntityRegistryService::ViewAnimators() const
{
    RebuildViews();
    return m_CachedAnimators;
}

const std::vector<EntityID>& EntityRegistryService::ViewRelationships() const
{
    RebuildViews();
    return m_CachedRelationships;
}