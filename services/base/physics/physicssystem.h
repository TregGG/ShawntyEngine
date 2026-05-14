#pragma once

#include <vector>
#include <memory>
#include "../../service.h"
#include <glm/vec2.hpp>

// Forward declarations
class ColliderComponent;
class RigidBodyComponent;
class QuadtreeNode;

// ============================
// Collision Event
// ============================
struct CollisionEvent
{
    ColliderComponent* a;
    ColliderComponent* b;
    glm::vec2 normal {0.0f}; // normal from a to b
    float depth = 0.0f;

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

    void RegisterRigidBody(RigidBodyComponent* rb);
    void UnregisterRigidBody(RigidBodyComponent* rb);

    // ========================
    // Collision Queries
    // ========================
    
    // Check if object collided with anything
    bool HasCollision(ColliderComponent* obj) const;

    // Check specific pair
    bool IsColliding(ColliderComponent* a, ColliderComponent* b) const;
    
    // Explicit Ray query
    // Runs mathematical projection natively evaluating overlaps independently across registered nodes
    bool Raycast(const glm::vec2& start, const glm::vec2& dir, float length, RaycastHit& outHit, uint32_t layerMask = 0xFFFFFFFF, ColliderComponent* ignoreCollider = nullptr, bool hitTriggers = false) const;
    
    // Shape Casts (Swept Volumes)
    bool CircleCast(const glm::vec2& start, const glm::vec2& end, float radius, RaycastHit& outHit, uint32_t layerMask = 0xFFFFFFFF, ColliderComponent* ignoreCollider = nullptr, bool hitTriggers = false) const;
    bool BoxCast(const glm::vec2& start, const glm::vec2& end, const glm::vec2& size, RaycastHit& outHit, uint32_t layerMask = 0xFFFFFFFF, ColliderComponent* ignoreCollider = nullptr, bool hitTriggers = false) const;
    
    // Trigger separation methods
    bool HasSolidCollision(ColliderComponent* obj) const;
    std::vector<ColliderComponent*> GetOverlappingTriggers(ColliderComponent* obj) const;

    // Get all collisions (read-only)
    const std::vector<CollisionEvent>& GetCollisions() const;

private:
    void CheckCollisions();
    void DispatchEvents();
    bool CalculateManifold(ColliderComponent* a, ColliderComponent* b, glm::vec2& outNormal, float& outDepth);

private:

    // Working set (per frame)
    std::vector<ColliderComponent*> m_Colliders;
    std::vector<RigidBodyComponent*> m_RigidBodies;

    // Collision results
    std::vector<CollisionEvent> m_PreviousCollisions;
    std::vector<CollisionEvent> m_Collisions;
    
    // Quadtree instance
    std::unique_ptr<QuadtreeNode> m_Quadtree;
};