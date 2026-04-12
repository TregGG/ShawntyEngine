#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <string_view>
#include "../../../core/entityid.h"
#include "../../service.h"

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