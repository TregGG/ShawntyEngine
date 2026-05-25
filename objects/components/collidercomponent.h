#pragma once
#include "component.h"
#include "../gameobject.h"
#include <glm/vec2.hpp>
#include <functional>
#include "components.h"

class ColliderComponent
{
public:
    struct AABB {
        float minX, minY;
        float maxX, maxY;
    };
    
    // We pass the EntityID of the other entity involved in the collision.
    using TriggerCallback = std::function<void(EntityID self, EntityID other)>;

    ColliderComponent(const glm::vec2& localOffset = glm::vec2(0.0f), 
                      const glm::vec2& localSize = glm::vec2(1.0f),
                      bool isTrigger = false)
        : m_LocalOffset(localOffset), m_LocalSize(localSize), m_IsTrigger(isTrigger) {}

    uint32_t GetLayerMask() const { return m_LayerMask; }
    void SetLayerMask(uint32_t mask) { m_LayerMask = mask; }

    bool IsTrigger() const { return m_IsTrigger; }
    void SetTrigger(bool trigger) { m_IsTrigger = trigger; }

    bool GetAutoBounds() const { return m_AutoBounds; }
    void SetAutoBounds(bool autoBounds) { m_AutoBounds = autoBounds; }

    void SetOnTriggerEnter(TriggerCallback callback) { m_OnTriggerEnter = std::move(callback); }
    const TriggerCallback& GetOnTriggerEnter() const { return m_OnTriggerEnter; }

    void SetOnTriggerExit(TriggerCallback callback) { m_OnTriggerExit = std::move(callback); }
    const TriggerCallback& GetOnTriggerExit() const { return m_OnTriggerExit; }

    // Dynamic AABB calculation based on given transform
    AABB GetBounds(const TransformComponent& transform) const {
        AABB bounds = {0.0f, 0.0f, 0.0f, 0.0f};
        
        glm::vec2 worldPos = transform.GetWorldPosition();
        float worldX = worldPos.x;
        float worldY = worldPos.y;
        float worldW = transform.size.x;
        float worldH = transform.size.y;

        if (!m_AutoBounds) {
            worldX += m_LocalOffset.x;
            worldY += m_LocalOffset.y;
            worldW *= m_LocalSize.x;
            worldH *= m_LocalSize.y;
        }

        // Assuming position is the center of the object
        bounds.minX = worldX - (worldW * 0.5f);
        bounds.maxX = worldX + (worldW * 0.5f);
        bounds.minY = worldY - (worldH * 0.5f);
        bounds.maxY = worldY + (worldH * 0.5f);
        
        return bounds;
    }

private:
    glm::vec2 m_LocalOffset;
    glm::vec2 m_LocalSize;
    bool m_IsTrigger;
    bool m_AutoBounds = false;
    uint32_t m_LayerMask = 0xFFFFFFFF; // Defaults to accepting all collisions
    TriggerCallback m_OnTriggerEnter;
    TriggerCallback m_OnTriggerExit;
};
