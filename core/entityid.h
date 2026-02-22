#pragma once

#include <cstdint>

using EntityID = std::uint64_t;
// EntityID is a 64-bit value where the lower 32 bits represent the entity index and the upper 32 bits represent the generation.

inline std::uint32_t EntityIndex(EntityID id)
{
    return static_cast<std::uint32_t>(id & 0xFFFFFFFF);
}

inline std::uint32_t EntityGeneration(EntityID id)
{
    return static_cast<std::uint32_t>((id >> 32) & 0xFFFFFFFF);
}

inline EntityID MakeEntityID(std::uint32_t index, std::uint32_t generation)
{
    return (static_cast<std::uint64_t>(generation) << 32) |
           static_cast<std::uint64_t>(index);
}
