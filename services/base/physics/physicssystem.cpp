#include "physicssystem.h"
#define ENGINE_CLASS "PhysicsSystem"
#include "../../../core/enginedebug.h"
#include "../../../objects/components/collidercomponent.h"
#include "../../../objects/components/rigidbodycomponent.h"
#include "../raycast.h"
#include <glm/geometric.hpp>
#include <algorithm>

// ============================
// Internal Quadtree Node
// ============================
class QuadtreeNode {
public:
    ColliderComponent::AABB bounds;
    std::vector<ColliderComponent*> colliders;
    std::unique_ptr<QuadtreeNode> children[4];
    bool isLeaf = true;
    
    // Limits
    static constexpr int MAX_OBJECTS = 4;
    static constexpr int MAX_LEVELS = 5;
    int level = 0;

    QuadtreeNode(int pLevel, ColliderComponent::AABB pBounds)
        : bounds(pBounds), level(pLevel) {}

    void Clear() {
        colliders.clear();
        for (int i = 0; i < 4; i++) {
            if (children[i]) {
                children[i]->Clear();
                children[i].reset();
            }
        }
        isLeaf = true;
    }

    void Split() {
        float subWidth = (bounds.maxX - bounds.minX) / 2.0f;
        float subHeight = (bounds.maxY - bounds.minY) / 2.0f;
        float x = bounds.minX;
        float y = bounds.minY;

        children[0] = std::make_unique<QuadtreeNode>(level + 1, ColliderComponent::AABB{x + subWidth, y, x + subWidth * 2, y + subHeight}); // Q1
        children[1] = std::make_unique<QuadtreeNode>(level + 1, ColliderComponent::AABB{x, y, x + subWidth, y + subHeight}); // Q2
        children[2] = std::make_unique<QuadtreeNode>(level + 1, ColliderComponent::AABB{x, y + subHeight, x + subWidth, y + subHeight * 2}); // Q3
        children[3] = std::make_unique<QuadtreeNode>(level + 1, ColliderComponent::AABB{x + subWidth, y + subHeight, x + subWidth * 2, y + subHeight * 2}); // Q4

        isLeaf = false;
    }

    int GetIndex(ColliderComponent* collider) {
        int index = -1;
        auto cbounds = collider->GetBounds();
        double verticalMidpoint = bounds.minX + (bounds.maxX - bounds.minX) / 2.0;
        double horizontalMidpoint = bounds.minY + (bounds.maxY - bounds.minY) / 2.0;

        bool topQuadrant = (cbounds.minY < horizontalMidpoint && cbounds.maxY < horizontalMidpoint);
        bool bottomQuadrant = (cbounds.minY > horizontalMidpoint);
        bool leftQuadrant = (cbounds.minX < verticalMidpoint && cbounds.maxX < verticalMidpoint);
        bool rightQuadrant = (cbounds.minX > verticalMidpoint);

        if (leftQuadrant) {
            if (topQuadrant) index = 1;      // Q2
            else if (bottomQuadrant) index = 2; // Q3
        } else if (rightQuadrant) {
            if (topQuadrant) index = 0;      // Q1
            else if (bottomQuadrant) index = 3; // Q4
        }
        return index;
    }

    void Insert(ColliderComponent* collider) {
        if (!isLeaf) {
            int index = GetIndex(collider);
            if (index != -1) {
                children[index]->Insert(collider);
                return;
            }
        }

        colliders.push_back(collider);

        if (colliders.size() > MAX_OBJECTS && level < MAX_LEVELS) {
            if (isLeaf) Split();
            
            auto it = colliders.begin();
            while (it != colliders.end()) {
                int index = GetIndex(*it);
                if (index != -1) {
                    children[index]->Insert(*it);
                    it = colliders.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    std::vector<ColliderComponent*> Retrieve(std::vector<ColliderComponent*>& returnObjects, ColliderComponent* collider) {
        int index = GetIndex(collider);
        if (index != -1 && !isLeaf) {
            children[index]->Retrieve(returnObjects, collider);
        }
        returnObjects.insert(returnObjects.end(), colliders.begin(), colliders.end());
        return returnObjects;
    }
};

// ============================
// Physics System
// ============================
PhysicsSystem::PhysicsSystem() {}
PhysicsSystem::~PhysicsSystem() {}

void PhysicsSystem::Init() {
    // Initializing quadtree with highly expansive bounds suitable for typical 2D
    m_Quadtree = std::make_unique<QuadtreeNode>(0, ColliderComponent::AABB{-25000.0f, -25000.0f, 25000.0f, 25000.0f});
    ENGINE_LOG("Quadtree initialized for spatial partitioning");
}

void PhysicsSystem::Update(float dt) {
    // 1. Integration (Positional progression)
    const glm::vec2 GRAVITY(0.0f, -9.81f);
    for (auto rb : m_RigidBodies) {
        if (!rb->IsEnabled() || !rb->GetOwner()->IsActive()) continue;
        if (rb->GetType() == BodyType::Static) continue;

        if (rb->GetUseGravity()) {
            rb->AddForce(GRAVITY * rb->GetGravityScale() * rb->GetMass()); 
        }
        glm::vec2 vel = rb->GetVelocity();
        vel += rb->GetAcceleration() * dt;
        vel *= (1.0f - rb->GetDrag() * dt);
        rb->SetVelocity(vel);
        
        rb->GetOwner()->GetTransform().position += vel * dt;
        rb->ClearForces();
    }

    // 2. Overlap calculations
    CheckCollisions();
    DispatchEvents();

    // 3. Mathematical Displacement (Avoiding intersections continuously)
    for (const auto& ev : m_Collisions) {
        if (ev.a->IsTrigger() || ev.b->IsTrigger()) continue;
        
        auto rbA = ev.a->GetOwner()->GetComponent<RigidBodyComponent>();
        auto rbB = ev.b->GetOwner()->GetComponent<RigidBodyComponent>();
        
        bool aMovable = rbA && rbA->GetType() == BodyType::Dynamic;
        bool bMovable = rbB && rbB->GetType() == BodyType::Dynamic;
        
        if (!aMovable && !bMovable) continue;
        
        // Positional Displacement
        if (aMovable && !bMovable) {
            ev.a->GetOwner()->GetTransform().position += ev.normal * ev.depth;
        } else if (!aMovable && bMovable) { 
            // Note: normal explicitly points A outward, so we invert testing -normal for B!
            ev.b->GetOwner()->GetTransform().position -= ev.normal * ev.depth;
        } else {
            // Both dynamic: divide depth natively avoiding clipping limits
            ev.a->GetOwner()->GetTransform().position += ev.normal * (ev.depth * 0.5f);
            ev.b->GetOwner()->GetTransform().position -= ev.normal * (ev.depth * 0.5f);
        }

        // Velocity Resolution (Elasticity)
        glm::vec2 velA = aMovable ? rbA->GetVelocity() : glm::vec2(0.0f);
        glm::vec2 velB = bMovable ? rbB->GetVelocity() : glm::vec2(0.0f);

        // Relative velocity (A relative to B)
        glm::vec2 rv = velA - velB;
        float vDot = glm::dot(rv, ev.normal);

        // If they are moving apart, do nothing
        if (vDot >= 0) continue;

        // Calculate combined elasticity (choose the highest bounce)
        float eA = rbA ? rbA->GetElasticity() : 0.0f;
        float eB = rbB ? rbB->GetElasticity() : 0.0f;
        float e = std::max(eA, eB);

        // Calculate inverse mass sum
        float invMassA = aMovable ? rbA->GetInverseMass() : 0.0f;
        float invMassB = bMovable ? rbB->GetInverseMass() : 0.0f;
        float invMassSum = invMassA + invMassB;

        if (invMassSum == 0.0f) continue; // Should not happen if aMovable or bMovable is true

        // Calculate impulse scalar
        float j = -(1.0f + e) * vDot / invMassSum;
        glm::vec2 impulse = j * ev.normal;

        if (aMovable) rbA->SetVelocity(velA + impulse * invMassA);
        if (bMovable) rbB->SetVelocity(velB - impulse * invMassB);
    }
}

void PhysicsSystem::Shutdown() {
    if (m_Quadtree) m_Quadtree->Clear();
    m_RigidBodies.clear();
    m_Colliders.clear();
    m_Collisions.clear();
    m_PreviousCollisions.clear();
}

void PhysicsSystem::RegisterCollider(ColliderComponent* collider) {
    auto it = std::find(m_Colliders.begin(), m_Colliders.end(), collider);
    if (it == m_Colliders.end()) {
        m_Colliders.push_back(collider);
    }
}

void PhysicsSystem::UnregisterCollider(ColliderComponent* collider) {
    auto it = std::find(m_Colliders.begin(), m_Colliders.end(), collider);
    if (it != m_Colliders.end()) {
        m_Colliders.erase(it);
    }
}

void PhysicsSystem::RegisterRigidBody(RigidBodyComponent* rb) {
    auto it = std::find(m_RigidBodies.begin(), m_RigidBodies.end(), rb);
    if (it == m_RigidBodies.end()) {
        m_RigidBodies.push_back(rb);
    }
}

void PhysicsSystem::UnregisterRigidBody(RigidBodyComponent* rb) {
    auto it = std::find(m_RigidBodies.begin(), m_RigidBodies.end(), rb);
    if (it != m_RigidBodies.end()) {
        m_RigidBodies.erase(it);
    }
}

bool PhysicsSystem::CalculateManifold(ColliderComponent* a, ColliderComponent* b, glm::vec2& outNormal, float& outDepth) {
    auto boundsA = a->GetBounds();
    auto boundsB = b->GetBounds();

    float overlapX = std::min(boundsA.maxX - boundsB.minX, boundsB.maxX - boundsA.minX);
    if (overlapX <= 0) return false;
    
    float overlapY = std::min(boundsA.maxY - boundsB.minY, boundsB.maxY - boundsA.minY);
    if (overlapY <= 0) return false;
    
    if (overlapX < overlapY) {
        outDepth = overlapX;
        // Pushes A safely outside logically
        outNormal = (boundsA.minX < boundsB.minX) ? glm::vec2(-1.0f, 0.0f) : glm::vec2(1.0f, 0.0f);
    } else {
        outDepth = overlapY;
        outNormal = (boundsA.minY < boundsB.minY) ? glm::vec2(0.0f, -1.0f) : glm::vec2(0.0f, 1.0f);
    }

    return true;
}

void PhysicsSystem::CheckCollisions() {
    m_PreviousCollisions = m_Collisions;
    m_Collisions.clear();
    if (m_Quadtree) {
        m_Quadtree->Clear();

        // 1. Re-build tree dynamically
        for (auto c : m_Colliders) {
            // Only check currently active components essentially
            if (c->IsEnabled()) {
                m_Quadtree->Insert(c);
            }
        }
    }

    std::vector<ColliderComponent*> potentialCollisions;
    
    // 2. Query against populated tree
    for (size_t i = 0; i < m_Colliders.size(); ++i) {
        if (!m_Colliders[i]->IsEnabled()) continue;
        
        potentialCollisions.clear();
        m_Quadtree->Retrieve(potentialCollisions, m_Colliders[i]);

        for (size_t j = 0; j < potentialCollisions.size(); ++j) {
            // Memory address ordering technique solves 'duplicate/self' pair handling intrinsically
            if (m_Colliders[i] < potentialCollisions[j]) {
                glm::vec2 normal;
                float depth;
                if (CalculateManifold(m_Colliders[i], potentialCollisions[j], normal, depth)) {
                    uint32_t layerBitA = 1 << static_cast<int>(m_Colliders[i]->GetOwner()->GetLayer());
                    uint32_t layerBitB = 1 << static_cast<int>(potentialCollisions[j]->GetOwner()->GetLayer());

                    if ((m_Colliders[i]->GetLayerMask() & layerBitB) != 0 && (potentialCollisions[j]->GetLayerMask() & layerBitA) != 0) {
                        m_Collisions.push_back({m_Colliders[i], potentialCollisions[j], normal, depth});
                    }
                }
            }
        }
    }
}

bool PhysicsSystem::HasCollision(ColliderComponent* obj) const {
    for (const auto& ev : m_Collisions) {
        if (ev.a == obj || ev.b == obj) return true;
    }
    return false;
}

bool PhysicsSystem::IsColliding(ColliderComponent* a, ColliderComponent* b) const {
    for (const auto& ev : m_Collisions) {
        if ((ev.a == a && ev.b == b) || (ev.a == b && ev.b == a)) return true;
    }
    return false;
}

void PhysicsSystem::DispatchEvents() {
    // Execute OnTriggerEnter by checking strictly for totally new overlap instances
    for (const auto& ev : m_Collisions) {
        auto it = std::find(m_PreviousCollisions.begin(), m_PreviousCollisions.end(), ev);
        if (it == m_PreviousCollisions.end()) {
            if (ev.a->IsTrigger()) {
                auto& cb = ev.a->GetOnTriggerEnter();
                if (cb) cb(ev.a, ev.b);
            }
            if (ev.b->IsTrigger()) {
                auto& cb = ev.b->GetOnTriggerEnter();
                if (cb) cb(ev.b, ev.a);
            }
        }
    }

    // Execute OnTriggerExit checking for expired overlaps missing from current frame
    for (const auto& ev : m_PreviousCollisions) {
        auto it = std::find(m_Collisions.begin(), m_Collisions.end(), ev);
        if (it == m_Collisions.end()) {
            if (ev.a->IsTrigger()) {
                auto& cb = ev.a->GetOnTriggerExit();
                if (cb) cb(ev.a, ev.b);
            }
            if (ev.b->IsTrigger()) {
                auto& cb = ev.b->GetOnTriggerExit();
                if (cb) cb(ev.b, ev.a);
            }
        }
    }
}

bool PhysicsSystem::HasSolidCollision(ColliderComponent* obj) const {
    if (obj->IsTrigger()) return false;

    for (const auto& ev : m_Collisions) {
        if (ev.a == obj && !ev.b->IsTrigger()) return true;
        if (ev.b == obj && !ev.a->IsTrigger()) return true;
    }
    return false;
}

std::vector<ColliderComponent*> PhysicsSystem::GetOverlappingTriggers(ColliderComponent* obj) const {
    std::vector<ColliderComponent*> triggers;
    for (const auto& ev : m_Collisions) {
        if (ev.a == obj && ev.b->IsTrigger()) triggers.push_back(ev.b);
        if (ev.b == obj && ev.a->IsTrigger()) triggers.push_back(ev.a);
    }
    return triggers;
}

const std::vector<CollisionEvent>& PhysicsSystem::GetCollisions() const {
    return m_Collisions;
}

// -------------------------------------------------------------
// Core Raycasting Algorithm (Slab Method natively parsing intersections seamlessly)
// -------------------------------------------------------------
static bool RayIntersectAABB(glm::vec2 start, glm::vec2 dir, float length, const ColliderComponent::AABB& aabb, float& out_t, glm::vec2& out_normal) {
    float tmin = 0.0f;
    float tmax = length;
    glm::vec2 normal(0.0f);
    
    // X Axis computation
    if (std::abs(dir.x) < 1e-6f) {
        if (start.x < aabb.minX || start.x > aabb.maxX) return false;
    } else {
        float invD = 1.0f / dir.x;
        float t1 = (aabb.minX - start.x) * invD;
        float t2 = (aabb.maxX - start.x) * invD;
        float sign = invD < 0.0f ? -1.0f : 1.0f;
        if (t1 > t2) std::swap(t1, t2);
        
        if (t1 > tmin) { tmin = t1; normal = glm::vec2(-sign, 0.0f); }
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }
    
    // Y Axis computation
    if (std::abs(dir.y) < 1e-6f) {
        if (start.y < aabb.minY || start.y > aabb.maxY) return false;
    } else {
        float invD = 1.0f / dir.y;
        float t1 = (aabb.minY - start.y) * invD;
        float t2 = (aabb.maxY - start.y) * invD;
        float sign = invD < 0.0f ? -1.0f : 1.0f;
        if (t1 > t2) std::swap(t1, t2);
        
        if (t1 > tmin) { tmin = t1; normal = glm::vec2(0.0f, -sign); }
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return false;
    }
    
    out_t = tmin;
    out_normal = normal;
    return true;
}

bool PhysicsSystem::Raycast(const glm::vec2& start, const glm::vec2& dir, float length, RaycastHit& outHit, uint32_t layerMask, ColliderComponent* ignoreCollider, bool hitTriggers) const {
    outHit.hit = false;
    outHit.distance = length;

    // Normalizing trajectory implicitly ensuring accurate metrics distances
    glm::vec2 ndir = glm::normalize(dir);

    // Simplistic loop over all actively tracked objects 
    // Quadtree optimization could be hooked here specifically later!
    for (auto* col : m_Colliders) {
        
        if (col == ignoreCollider) continue;
        if (!col->GetOwner() || !col->GetOwner()->IsActive()) continue;
        if (!hitTriggers && col->IsTrigger()) continue;

        uint32_t colLayerBit = 1 << static_cast<int>(col->GetOwner()->GetLayer());
        if ((layerMask & colLayerBit) == 0) continue;

        float t;
        glm::vec2 nRaw;
        
        if (RayIntersectAABB(start, ndir, length, col->GetBounds(), t, nRaw)) {
            if (t < outHit.distance) {
                outHit.hit = true;
                outHit.distance = t;
                outHit.point = start + ndir * t;
                outHit.normal = nRaw;
                outHit.collider = col;
            }
        }
    }

    return outHit.hit;
}
