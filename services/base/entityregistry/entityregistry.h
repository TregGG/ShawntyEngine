#pragma once

#include <vector>
#include <cstdint>
#include "../../../core/entityid.h"
#include "../../service.h"

class EntityRegistryService : public Service
{
public:
    EntityRegistryService() = default;
    ~EntityRegistryService() override = default;

    void Init() override {}
    void Update(float dt) override;
    void Shutdown() override;

    EntityID Create();
    void Destroy(EntityID entity);
    bool IsAlive(EntityID entity) const;

private:
    struct Slot
    {
        std::uint32_t generation = 0;
        bool alive = false;
    };

    std::vector<Slot> m_Slots;
    std::vector<std::uint32_t> m_FreeList;
    std::vector<std::uint32_t> m_PendingDestroy;
};
