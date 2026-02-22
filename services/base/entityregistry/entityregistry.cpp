#include "entityregistry.h"

EntityID EntityRegistryService::Create()
{
    std::uint32_t index;

    if (!m_FreeList.empty())
    {
        index = m_FreeList.back();
        m_FreeList.pop_back();

        Slot& slot = m_Slots[index];
        slot.alive = true;

        return MakeEntityID(index, slot.generation);
    }
    else
    {
        index = static_cast<std::uint32_t>(m_Slots.size());

        Slot slot;
        slot.generation = 0;
        slot.alive = true;

        m_Slots.push_back(slot);

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

void EntityRegistryService::Update(float /*dt*/)
{
    // Process deferred destruction
    for (std::uint32_t index : m_PendingDestroy)
    {
        Slot& slot = m_Slots[index];

        slot.alive = false;
        slot.generation++; // Invalidate old IDs

        m_FreeList.push_back(index);
    }

    m_PendingDestroy.clear();
}

void EntityRegistryService::Shutdown()
{
    m_Slots.clear();
    m_FreeList.clear();
    m_PendingDestroy.clear();
}
