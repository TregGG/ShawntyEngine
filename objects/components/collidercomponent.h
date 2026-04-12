#pragma once
#include "component.h"
#include "../gameobject.h"
#include <glm/vec2.hpp>

class ColliderComponent : public Component
{
public:
    struct AABB {
        float minX, minY;
        float maxX, maxY;
    };

    ColliderComponent(const glm::vec2& localOffset = glm::vec2(0.0f), const glm::vec2& localSize = glm::vec2(1.0f))
        : m_LocalOffset(localOffset), m_LocalSize(localSize) {}

    // Empty override as physics computes everything dynamically on request
    void Update(float /*deltatime*/) override {}

    // Dynamic AABB calculation based on parent transform
    AABB GetBounds() const {
        AABB bounds = {0.0f, 0.0f, 0.0f, 0.0f};
        if (m_Owner) {
            const auto& transform = m_Owner->GetTransform();
            
            float worldX = transform.position.x + m_LocalOffset.x;
            float worldY = transform.position.y + m_LocalOffset.y;
            float worldW = transform.size.x * m_LocalSize.x;
            float worldH = transform.size.y * m_LocalSize.y;

            // Assuming position is the center of the object
            bounds.minX = worldX - (worldW * 0.5f);
            bounds.maxX = worldX + (worldW * 0.5f);
            bounds.minY = worldY - (worldH * 0.5f);
            bounds.maxY = worldY + (worldH * 0.5f);
        }
        return bounds;
    }

private:
    glm::vec2 m_LocalOffset;
    glm::vec2 m_LocalSize;
};
