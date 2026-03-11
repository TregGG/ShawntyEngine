#include "entityregistry.h"

void EntityRegistryService::Init()
{
    m_CategoryBuckets.resize(static_cast<size_t>(EntityCategory::Count));
}

EntityID EntityRegistryService::Create(EntityCategory category)
{
    std::uint32_t index;

    if (!m_FreeList.empty())
    {
        index = m_FreeList.back();
        m_FreeList.pop_back();

        Slot& slot = m_Slots[index];
        slot.alive = true;
        slot.category = category;

        // Add to category bucket
        m_CategoryBuckets[(size_t)category].push_back(index);

        return MakeEntityID(index, slot.generation);
    }
    else
    {
        index = static_cast<std::uint32_t>(m_Slots.size());

        Slot slot;
        slot.generation = 0;
        slot.alive = true;
        slot.category = category;

        m_Slots.push_back(slot);

        // Add to category bucket
        m_CategoryBuckets[(size_t)category].push_back(index);

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

void EntityRegistryService::Update(float /*dt*/)
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

        m_FreeList.push_back(index);
    }

    m_PendingDestroy.clear();
}

void EntityRegistryService::Shutdown()
{
    m_Slots.clear();
    m_FreeList.clear();
    m_PendingDestroy.clear();
    m_CategoryBuckets.clear();
}