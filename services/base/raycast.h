#pragma once
#include <glm/vec2.hpp>

class ColliderComponent;

struct RaycastHit {
    bool hit = false;
    glm::vec2 point = glm::vec2(0.0f);
    glm::vec2 normal = glm::vec2(0.0f);
    float distance = 0.0f;
    ColliderComponent* collider = nullptr; // Note: Ensure PhysicsSystem processes hits properly!
};

// Exposes explicit global macro layout for ease of usage across gameplay!
#define RAYCAST(startPos, dir, length, outHit) \
    m_Physics.Raycast(startPos, dir, length, outHit)

#define RAYCAST_IGNORE(startPos, dir, length, outHit, ignoreComp) \
    m_Physics.Raycast(startPos, dir, length, outHit, 0xFFFFFFFF, ignoreComp)

#define RAYCAST_MASK(startPos, dir, length, outHit, mask) \
    m_Physics.Raycast(startPos, dir, length, outHit, mask)

#define RAYCAST_IGNORE_MASK(startPos, dir, length, outHit, ignoreComp, mask) \
    m_Physics.Raycast(startPos, dir, length, outHit, mask, ignoreComp)

#define CIRCLE_CAST(start, end, radius, out, ignore_layer) \
    m_Physics.CircleCast(start, end, radius, out, ignore_layer)

#define BOX_CAST(start, end, size, out, ignore_layer) \
    m_Physics.BoxCast(start, end, size, out, ignore_layer)
