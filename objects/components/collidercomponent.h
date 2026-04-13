#pragma once
#include "component.h"
#include "../gameobject.h"
#include <glm/vec2.hpp>
#include <functional>

class ColliderComponent : public Component
{
public:
    struct AABB {
        float minX, minY;
        float maxX, maxY;
    };
    
    using TriggerCallback = std::function<void(ColliderComponent* self, ColliderComponent* other)>;

    ColliderComponent(const glm::vec2& localOffset = glm::vec2(0.0f), 
                      const glm::vec2& localSize = glm::vec2(1.0f),
                      bool isTrigger = false)
        : m_LocalOffset(localOffset), m_LocalSize(localSize), m_IsTrigger(isTrigger) {}

    // Empty override as physics computes everything dynamically on request
    void Update(float /*deltatime*/) override {}

    bool IsTrigger() const { return m_IsTrigger; }
    void SetTrigger(bool trigger) { m_IsTrigger = trigger; }

    bool GetAutoBounds() const { return m_AutoBounds; }
    void SetAutoBounds(bool autoBounds) { m_AutoBounds = autoBounds; }

    void SetOnTriggerEnter(TriggerCallback callback) { m_OnTriggerEnter = std::move(callback); }
    const TriggerCallback& GetOnTriggerEnter() const { return m_OnTriggerEnter; }

    void SetOnTriggerExit(TriggerCallback callback) { m_OnTriggerExit = std::move(callback); }
    const TriggerCallback& GetOnTriggerExit() const { return m_OnTriggerExit; }

    // Dynamic AABB calculation based on parent transform
    AABB GetBounds() const {
        AABB bounds = {0.0f, 0.0f, 0.0f, 0.0f};
        if (m_Owner) {
            const auto& transform = m_Owner->GetTransform();
            
            float worldX = transform.position.x;
            float worldY = transform.position.y;
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
        }
        return bounds;
    }

private:
    glm::vec2 m_LocalOffset;
    glm::vec2 m_LocalSize;
    bool m_IsTrigger;
    bool m_AutoBounds = false;
    TriggerCallback m_OnTriggerEnter;
    TriggerCallback m_OnTriggerExit;
};
