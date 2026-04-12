#pragma once

#include <vector>
#include "../../service.h"

// Forward declarations
class ColliderComponent;

// ============================
// Collision Event
// ============================
struct CollisionEvent
{
    ColliderComponent* a;
    ColliderComponent* b;
};

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

    // Get all collisions (read-only)
    const std::vector<CollisionEvent>& GetCollisions() const;

private:

    void CheckCollisions();
    void ResolveCollisions(); // (empty for now, future use)

    // AABB collision test
    bool Intersects(ColliderComponent* a, ColliderComponent* b);

private:

    // Working set (per frame)
    std::vector<ColliderComponent*> m_Colliders;

    // Collision results
    std::vector<CollisionEvent> m_Collisions;
};