#pragma once

#include <vector>
#include <memory>
#include "../../service.h"
#include <glm/vec2.hpp>

// Forward declarations
class ColliderComponent;
class QuadtreeNode;

// ============================
// Collision Event
// ============================
struct CollisionEvent
{
    ColliderComponent* a;
    ColliderComponent* b;

    bool operator==(const CollisionEvent& other) const {
        return (a == other.a && b == other.b) || (a == other.b && b == other.a);
    }
};

// Forward declare explicitly handling queries independently
struct RaycastHit;

// ============================
// Physics System
// ============================
class PhysicsSystem : public Service
{
public:
    PhysicsSystem();
    ~PhysicsSystem() override;

    void Init() override;
    void Update(float dt) override;
    void Shutdown() override;

    void RegisterCollider(ColliderComponent* collider);
    void UnregisterCollider(ColliderComponent* collider);

    // ========================
    // Collision Queries
    // ========================
    
    // Check if object collided with anything
    bool HasCollision(ColliderComponent* obj) const;

    // Check specific pair
    bool IsColliding(ColliderComponent* a, ColliderComponent* b) const;
    
    // Explicit Ray query
    // Runs mathematical projection natively evaluating overlaps independently across registered nodes
    bool Raycast(const glm::vec2& start, const glm::vec2& dir, float length, RaycastHit& outHit, ColliderComponent* ignoreCollider = nullptr, bool hitTriggers = false) const;
    
    // Trigger separation methods
    bool HasSolidCollision(ColliderComponent* obj) const;
    std::vector<ColliderComponent*> GetOverlappingTriggers(ColliderComponent* obj) const;

    // Get all collisions (read-only)
    const std::vector<CollisionEvent>& GetCollisions() const;

private:
    void CheckCollisions();
    void DispatchEvents();
    bool Intersects(ColliderComponent* a, ColliderComponent* b);

private:

    // Working set (per frame)
    std::vector<ColliderComponent*> m_Colliders;

    // Collision results
    std::vector<CollisionEvent> m_PreviousCollisions;
    std::vector<CollisionEvent> m_Collisions;
    
    // Quadtree instance
    std::unique_ptr<QuadtreeNode> m_Quadtree;
};