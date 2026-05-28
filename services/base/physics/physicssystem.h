#pragma once

#include <vector>
#include <memory>
#include "../../service.h"
#include <glm/vec2.hpp>
#include "../../../core/entityid.h"

// Forward declarations
class ColliderComponent;
class RigidBodyComponent;
class QuadtreeNode;

// ============================
// Collision Event
// ============================
struct CollisionEvent
{
    EntityID a;
    EntityID b;
    glm::vec2 normal {0.0f}; // normal from a to b
    float depth = 0.0f;

    bool operator==(const CollisionEvent& other) const {
        return (a == other.a && b == other.b) || (a == other.b && b == other.a);
    }
};

struct RaycastHit;

class EntityRegistryService;

class PhysicsSystem : public Service
{
public:
    PhysicsSystem();
    ~PhysicsSystem() override;

    void Init() override;
    void Update(float dt) override;
    void Shutdown() override;

    void BindRegistry(EntityRegistryService* registry) { m_Registry = registry; }

    // ========================
    // Collision Queries
    // ========================
    
    // Check if object collided with anything
    bool HasCollision(EntityID obj) const;

    // Check specific pair
    bool IsColliding(EntityID a, EntityID b) const;
    
    // Explicit Ray query
    bool Raycast(const glm::vec2& start, const glm::vec2& dir, float length, RaycastHit& outHit, uint32_t layerMask = 0xFFFFFFFF, EntityID ignoreEntity = 0, bool hitTriggers = false) const;
    
    // Shape Casts
    bool CircleCast(const glm::vec2& start, const glm::vec2& end, float radius, RaycastHit& outHit, uint32_t layerMask = 0xFFFFFFFF, EntityID ignoreEntity = 0, bool hitTriggers = false) const;
    bool BoxCast(const glm::vec2& start, const glm::vec2& end, const glm::vec2& size, RaycastHit& outHit, uint32_t layerMask = 0xFFFFFFFF, EntityID ignoreEntity = 0, bool hitTriggers = false) const;
    
    // Trigger separation methods
    bool HasSolidCollision(EntityID obj) const;
    std::vector<EntityID> GetOverlappingTriggers(EntityID obj) const;

    const std::vector<CollisionEvent>& GetCollisions() const;

    bool IsPlayerPlayerPushingPrevented() const { return m_PreventPlayerPlayerPushing; }
    void SetPreventPlayerPlayerPushing(bool prevent) { m_PreventPlayerPlayerPushing = prevent; }

    bool IsPlayersTransparent() const { return m_PlayersTransparent; }
    void SetPlayersTransparent(bool transparent) { m_PlayersTransparent = transparent; }

private:
    void CheckCollisions();
    void DispatchEvents();
    bool CalculateManifold(EntityID a, EntityID b, glm::vec2& outNormal, float& outDepth);

private:

    EntityRegistryService* m_Registry = nullptr;

    // Collision results
    std::vector<CollisionEvent> m_PreviousCollisions;
    std::vector<CollisionEvent> m_Collisions;
    
    std::unique_ptr<QuadtreeNode> m_Quadtree;
    bool m_PreventPlayerPlayerPushing = true;
    bool m_PlayersTransparent = false;
};