#include "physicssystem.h"
#define ENGINE_CLASS "PhysicsSystem"
#include "../../../core/enginedebug.h"
#include "../../../objects/components/collidercomponent.h"
#include "../../../objects/components/rigidbodycomponent.h"
#include "../raycast.h"
#include <glm/geometric.hpp>
#include <algorithm>

#include "../entityregistry/entityregistry.h"

// ============================
// Internal Quadtree Node
// ============================
class QuadtreeNode {
public:
    ColliderComponent::AABB bounds;
    std::vector<EntityID> colliders;
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

    int GetIndex(EntityID colliderEntity, EntityRegistryService* reg) {
        int index = -1;
        const auto& col = reg->GetComponent<ColliderComponent>(colliderEntity);
        const auto& trans = reg->GetComponent<TransformComponent>(colliderEntity);
        auto cbounds = col.GetBounds(trans);
        
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

    void Insert(EntityID colliderEntity, EntityRegistryService* reg) {
        if (!isLeaf) {
            int index = GetIndex(colliderEntity, reg);
            if (index != -1) {
                children[index]->Insert(colliderEntity, reg);
                return;
            }
        }

        colliders.push_back(colliderEntity);

        if (colliders.size() > MAX_OBJECTS && level < MAX_LEVELS) {
            if (isLeaf) Split();
            
            auto it = colliders.begin();
            while (it != colliders.end()) {
                int index = GetIndex(*it, reg);
                if (index != -1) {
                    children[index]->Insert(*it, reg);
                    it = colliders.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    std::vector<EntityID>& Retrieve(std::vector<EntityID>& returnObjects, EntityID colliderEntity, EntityRegistryService* reg) {
        int index = GetIndex(colliderEntity, reg);
        if (index != -1 && !isLeaf) {
            children[index]->Retrieve(returnObjects, colliderEntity, reg);
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
    if (!m_Registry) return;

    // 1. Integration (Positional progression)
    const glm::vec2 GRAVITY(0.0f, -9.81f);
    
    // We iterate over entities that have RigidBody and Transform
    for (EntityID e : m_Registry->ViewPhysicsObjects()) {
        if (!m_Registry->HasComponent<RigidBodyComponent>(e)) continue;

        auto& rb = m_Registry->GetComponent<RigidBodyComponent>(e);
        auto& transform = m_Registry->GetComponent<TransformComponent>(e);
        
        if (rb.GetType() == BodyType::Static) continue;

        if (rb.GetUseGravity()) {
            rb.AddForce(GRAVITY * rb.GetGravityScale() * rb.GetMass()); 
        }
        glm::vec2 vel = rb.GetVelocity();
        vel += rb.GetAcceleration() * dt;
        vel *= (1.0f - rb.GetDrag() * dt);
        rb.SetVelocity(vel);
        
        transform.position += vel * dt;
        rb.ClearForces();
    }

    // 2. Overlap calculations
    CheckCollisions();
    DispatchEvents();

    // 3. Mathematical Displacement (Avoiding intersections continuously)
    for (const auto& ev : m_Collisions) {
        auto& colA = m_Registry->GetComponent<ColliderComponent>(ev.a);
        auto& colB = m_Registry->GetComponent<ColliderComponent>(ev.b);
        
        if (colA.IsTrigger() || colB.IsTrigger()) continue;
        
        bool hasRbA = m_Registry->HasComponent<RigidBodyComponent>(ev.a);
        bool hasRbB = m_Registry->HasComponent<RigidBodyComponent>(ev.b);
        
        RigidBodyComponent* rbA = hasRbA ? &m_Registry->GetComponent<RigidBodyComponent>(ev.a) : nullptr;
        RigidBodyComponent* rbB = hasRbB ? &m_Registry->GetComponent<RigidBodyComponent>(ev.b) : nullptr;
        
        bool aMovable = rbA && rbA->GetType() == BodyType::Dynamic;
        bool bMovable = rbB && rbB->GetType() == BodyType::Dynamic;
        
        if (!aMovable && !bMovable) continue;
        
        auto& transA = m_Registry->GetComponent<TransformComponent>(ev.a);
        auto& transB = m_Registry->GetComponent<TransformComponent>(ev.b);

        // Positional Displacement
        if (aMovable && !bMovable) {
            transA.position += ev.normal * ev.depth;
        } else if (!aMovable && bMovable) { 
            // Note: normal explicitly points A outward, so we invert testing -normal for B!
            transB.position -= ev.normal * ev.depth;
        } else {
            // Both dynamic: divide depth natively avoiding clipping limits
            transA.position += ev.normal * (ev.depth * 0.5f);
            transB.position -= ev.normal * (ev.depth * 0.5f);
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
    m_Collisions.clear();
    m_PreviousCollisions.clear();
}

bool PhysicsSystem::CalculateManifold(EntityID a, EntityID b, glm::vec2& outNormal, float& outDepth) {
    if (!m_Registry) return false;
    const auto& colA = m_Registry->GetComponent<ColliderComponent>(a);
    const auto& transA = m_Registry->GetComponent<TransformComponent>(a);
    const auto& colB = m_Registry->GetComponent<ColliderComponent>(b);
    const auto& transB = m_Registry->GetComponent<TransformComponent>(b);

    auto boundsA = colA.GetBounds(transA);
    auto boundsB = colB.GetBounds(transB);

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
    if (!m_Registry) return;

    std::vector<EntityID> colliders = m_Registry->ViewPhysicsObjects();

    if (m_Quadtree) {
        m_Quadtree->Clear();

        // 1. Re-build tree dynamically
        for (auto e : colliders) {
            m_Quadtree->Insert(e, m_Registry);
        }
    }

    std::vector<EntityID> potentialCollisions;
    
    // 2. Query against populated tree
    for (size_t i = 0; i < colliders.size(); ++i) {
        
        potentialCollisions.clear();
        m_Quadtree->Retrieve(potentialCollisions, colliders[i], m_Registry);

        for (size_t j = 0; j < potentialCollisions.size(); ++j) {
            // ID ordering technique solves 'duplicate/self' pair handling intrinsically
            if (colliders[i] < potentialCollisions[j]) {
                glm::vec2 normal;
                float depth;
                if (CalculateManifold(colliders[i], potentialCollisions[j], normal, depth)) {
                    
                    const auto& colA = m_Registry->GetComponent<ColliderComponent>(colliders[i]);
                    const auto& colB = m_Registry->GetComponent<ColliderComponent>(potentialCollisions[j]);

                    // Layer handling. Assuming layer is now in SpriteComponent or we have a default. 
                    // Let's use Layer from SpriteComponent if available, otherwise Layer::Foreground.
                    Layer layerA = Layer::Foreground;
                    Layer layerB = Layer::Foreground;
                    if (m_Registry->HasComponent<SpriteComponent2D>(colliders[i])) layerA = m_Registry->GetComponent<SpriteComponent2D>(colliders[i]).layer;
                    if (m_Registry->HasComponent<SpriteComponent2D>(potentialCollisions[j])) layerB = m_Registry->GetComponent<SpriteComponent2D>(potentialCollisions[j]).layer;
                    
                    uint32_t layerBitA = 1 << static_cast<int>(layerA);
                    uint32_t layerBitB = 1 << static_cast<int>(layerB);

                    if ((colA.GetLayerMask() & layerBitB) != 0 && (colB.GetLayerMask() & layerBitA) != 0) {
                        m_Collisions.push_back({colliders[i], potentialCollisions[j], normal, depth});
                    }
                }
            }
        }
    }
}

bool PhysicsSystem::HasCollision(EntityID obj) const {
    for (const auto& ev : m_Collisions) {
        if (ev.a == obj || ev.b == obj) return true;
    }
    return false;
}

bool PhysicsSystem::IsColliding(EntityID a, EntityID b) const {
    for (const auto& ev : m_Collisions) {
        if ((ev.a == a && ev.b == b) || (ev.a == b && ev.b == a)) return true;
    }
    return false;
}

void PhysicsSystem::DispatchEvents() {
    if (!m_Registry) return;

    // Execute OnTriggerEnter by checking strictly for totally new overlap instances
    for (const auto& ev : m_Collisions) {
        auto it = std::find(m_PreviousCollisions.begin(), m_PreviousCollisions.end(), ev);
        if (it == m_PreviousCollisions.end()) {
            if (m_Registry->HasComponent<ColliderComponent>(ev.a)) {
                auto& colA = m_Registry->GetComponent<ColliderComponent>(ev.a);
                if (colA.IsTrigger()) {
                    auto& cb = colA.GetOnTriggerEnter();
                    if (cb) cb(ev.a, ev.b);
                }
            }
            if (m_Registry->HasComponent<ColliderComponent>(ev.b)) {
                auto& colB = m_Registry->GetComponent<ColliderComponent>(ev.b);
                if (colB.IsTrigger()) {
                    auto& cb = colB.GetOnTriggerEnter();
                    if (cb) cb(ev.b, ev.a);
                }
            }
        }
    }

    // Execute OnTriggerExit checking for expired overlaps missing from current frame
    for (const auto& ev : m_PreviousCollisions) {
        auto it = std::find(m_Collisions.begin(), m_Collisions.end(), ev);
        if (it == m_Collisions.end()) {
            if (m_Registry->HasComponent<ColliderComponent>(ev.a)) {
                auto& colA = m_Registry->GetComponent<ColliderComponent>(ev.a);
                if (colA.IsTrigger()) {
                    auto& cb = colA.GetOnTriggerExit();
                    if (cb) cb(ev.a, ev.b);
                }
            }
            if (m_Registry->HasComponent<ColliderComponent>(ev.b)) {
                auto& colB = m_Registry->GetComponent<ColliderComponent>(ev.b);
                if (colB.IsTrigger()) {
                    auto& cb = colB.GetOnTriggerExit();
                    if (cb) cb(ev.b, ev.a);
                }
            }
        }
    }
}

bool PhysicsSystem::HasSolidCollision(EntityID obj) const {
    if (!m_Registry) return false;
    const auto& objCol = m_Registry->GetComponent<ColliderComponent>(obj);
    if (objCol.IsTrigger()) return false;

    for (const auto& ev : m_Collisions) {
        if (ev.a == obj && !m_Registry->GetComponent<ColliderComponent>(ev.b).IsTrigger()) return true;
        if (ev.b == obj && !m_Registry->GetComponent<ColliderComponent>(ev.a).IsTrigger()) return true;
    }
    return false;
}

std::vector<EntityID> PhysicsSystem::GetOverlappingTriggers(EntityID obj) const {
    std::vector<EntityID> triggers;
    if (!m_Registry) return triggers;
    for (const auto& ev : m_Collisions) {
        if (ev.a == obj && m_Registry->GetComponent<ColliderComponent>(ev.b).IsTrigger()) triggers.push_back(ev.b);
        if (ev.b == obj && m_Registry->GetComponent<ColliderComponent>(ev.a).IsTrigger()) triggers.push_back(ev.a);
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

bool PhysicsSystem::Raycast(const glm::vec2& start, const glm::vec2& dir, float length, RaycastHit& outHit, uint32_t layerMask, EntityID ignoreEntity, bool hitTriggers) const {
    outHit.hit = false;
    outHit.distance = length;

    if (!m_Registry) return false;

    // Normalizing trajectory implicitly ensuring accurate metrics distances
    glm::vec2 ndir = glm::normalize(dir);

    // Simplistic loop over all actively tracked objects 
    // Quadtree optimization could be hooked here specifically later!
    for (EntityID e : m_Registry->ViewPhysicsObjects()) {
        
        if (e == ignoreEntity) continue;
        
        const auto& col = m_Registry->GetComponent<ColliderComponent>(e);
        const auto& trans = m_Registry->GetComponent<TransformComponent>(e);

        if (!hitTriggers && col.IsTrigger()) continue;

        Layer layer = Layer::Foreground;
        if (m_Registry->HasComponent<SpriteComponent2D>(e)) layer = m_Registry->GetComponent<SpriteComponent2D>(e).layer;

        uint32_t colLayerBit = 1 << static_cast<int>(layer);
        if ((layerMask & colLayerBit) == 0) continue;

        float t;
        glm::vec2 nRaw;
        
        if (RayIntersectAABB(start, ndir, length, col.GetBounds(trans), t, nRaw)) {
            if (t < outHit.distance) {
                outHit.hit = true;
                outHit.distance = t;
                outHit.point = start + ndir * t;
                outHit.normal = nRaw;
                outHit.entity = e;
            }
        }
    }

    return outHit.hit;
}

bool PhysicsSystem::BoxCast(const glm::vec2& start, const glm::vec2& end, const glm::vec2& size, RaycastHit& outHit, uint32_t layerMask, EntityID ignoreEntity, bool hitTriggers) const {
    outHit.hit = false;
    glm::vec2 diff = end - start;
    float length = glm::length(diff);
    if (length < 1e-6f) return false;
    if (!m_Registry) return false;
    
    glm::vec2 ndir = diff / length;
    outHit.distance = length;

    for (EntityID e : m_Registry->ViewPhysicsObjects()) {
        if (e == ignoreEntity) continue;
        
        const auto& col = m_Registry->GetComponent<ColliderComponent>(e);
        const auto& trans = m_Registry->GetComponent<TransformComponent>(e);

        if (!hitTriggers && col.IsTrigger()) continue;

        Layer layer = Layer::Foreground;
        if (m_Registry->HasComponent<SpriteComponent2D>(e)) layer = m_Registry->GetComponent<SpriteComponent2D>(e).layer;

        uint32_t colLayerBit = 1 << static_cast<int>(layer);
        if ((layerMask & colLayerBit) == 0) continue;

        // Expand bounds by box half extents (Minkowski Sum)
        ColliderComponent::AABB bounds = col.GetBounds(trans);
        bounds.minX -= size.x * 0.5f;
        bounds.maxX += size.x * 0.5f;
        bounds.minY -= size.y * 0.5f;
        bounds.maxY += size.y * 0.5f;

        // Check if start is inside expanded bounds
        if (start.x >= bounds.minX && start.x <= bounds.maxX &&
            start.y >= bounds.minY && start.y <= bounds.maxY) {
            outHit.hit = true;
            outHit.distance = 0.0f;
            outHit.point = start;
            outHit.normal = glm::vec2(0.0f, 1.0f);
            outHit.entity = e;
            continue;
        }

        float t;
        glm::vec2 nRaw;
        if (RayIntersectAABB(start, ndir, length, bounds, t, nRaw)) {
            if (t < outHit.distance) {
                outHit.hit = true;
                outHit.distance = t;
                // Calculate actual surface contact point rather than the center of the box
                glm::vec2 boxCenter = start + ndir * t;
                outHit.point = boxCenter - glm::vec2(nRaw.x * size.x * 0.5f, nRaw.y * size.y * 0.5f);
                outHit.normal = nRaw;
                outHit.entity = e;
            }
        }
    }
    return outHit.hit;
}

bool PhysicsSystem::CircleCast(const glm::vec2& start, const glm::vec2& end, float radius, RaycastHit& outHit, uint32_t layerMask, EntityID ignoreEntity, bool hitTriggers) const {
    outHit.hit = false;
    glm::vec2 diff = end - start;
    float length = glm::length(diff);
    if (length < 1e-6f) return false;
    if (!m_Registry) return false;
    
    glm::vec2 ndir = diff / length;
    outHit.distance = length;

    for (EntityID e : m_Registry->ViewPhysicsObjects()) {
        if (e == ignoreEntity) continue;
        
        const auto& col = m_Registry->GetComponent<ColliderComponent>(e);
        const auto& trans = m_Registry->GetComponent<TransformComponent>(e);

        if (!hitTriggers && col.IsTrigger()) continue;

        Layer layer = Layer::Foreground;
        if (m_Registry->HasComponent<SpriteComponent2D>(e)) layer = m_Registry->GetComponent<SpriteComponent2D>(e).layer;

        uint32_t colLayerBit = 1 << static_cast<int>(layer);
        if ((layerMask & colLayerBit) == 0) continue;

        ColliderComponent::AABB b = col.GetBounds(trans);
        
        // Distance from start to AABB
        float dx = std::max(0.0f, std::max(b.minX - start.x, start.x - b.maxX));
        float dy = std::max(0.0f, std::max(b.minY - start.y, start.y - b.maxY));
        if (dx*dx + dy*dy <= radius*radius) {
            outHit.hit = true;
            outHit.distance = 0.0f;
            outHit.point = start;
            outHit.normal = glm::vec2(0.0f, 1.0f);
            outHit.entity = e;
            continue;
        }

        float closestT = outHit.distance;
        bool gotHit = false;
        glm::vec2 hitNormal(0.0f);

        auto checkT = [&](float t, glm::vec2 n) {
            if (t >= 0.0f && t < closestT) {
                closestT = t;
                hitNormal = n;
                gotHit = true;
            }
        };

        // 4 Edges (shifted outward by radius)
        float tEdge;
        if (ndir.y < 0.0f) {
            tEdge = (b.maxY + radius - start.y) / ndir.y;
            if (tEdge >= 0.0f) {
                float px = start.x + ndir.x * tEdge;
                if (px >= b.minX && px <= b.maxX) checkT(tEdge, glm::vec2(0.0f, 1.0f));
            }
        }
        if (ndir.y > 0.0f) {
            tEdge = (b.minY - radius - start.y) / ndir.y;
            if (tEdge >= 0.0f) {
                float px = start.x + ndir.x * tEdge;
                if (px >= b.minX && px <= b.maxX) checkT(tEdge, glm::vec2(0.0f, -1.0f));
            }
        }
        if (ndir.x > 0.0f) {
            tEdge = (b.minX - radius - start.x) / ndir.x;
            if (tEdge >= 0.0f) {
                float py = start.y + ndir.y * tEdge;
                if (py >= b.minY && py <= b.maxY) checkT(tEdge, glm::vec2(-1.0f, 0.0f));
            }
        }
        if (ndir.x < 0.0f) {
            tEdge = (b.maxX + radius - start.x) / ndir.x;
            if (tEdge >= 0.0f) {
                float py = start.y + ndir.y * tEdge;
                if (py >= b.minY && py <= b.maxY) checkT(tEdge, glm::vec2(1.0f, 0.0f));
            }
        }

        // 4 Corner Circles
        glm::vec2 corners[4] = {
            glm::vec2(b.minX, b.minY),
            glm::vec2(b.maxX, b.minY),
            glm::vec2(b.minX, b.maxY),
            glm::vec2(b.maxX, b.maxY)
        };
        
        for (int i = 0; i < 4; ++i) {
            glm::vec2 m = start - corners[i];
            float B = glm::dot(m, ndir);
            float C = glm::dot(m, m) - radius * radius;
            if (C > 0.0f && B > 0.0f) continue;
            float discr = B * B - C;
            if (discr >= 0.0f) {
                float tCirc = -B - std::sqrt(discr);
                if (tCirc >= 0.0f) {
                    glm::vec2 nCirc = glm::normalize(start + ndir * tCirc - corners[i]);
                    checkT(tCirc, nCirc);
                }
            }
        }

        if (gotHit) {
            outHit.hit = true;
            outHit.distance = closestT;
            // Return exact surface contact point instead of circle center
            glm::vec2 circleCenter = start + ndir * closestT;
            outHit.point = circleCenter - hitNormal * radius;
            outHit.normal = hitNormal;
            outHit.entity = e;
        }
    }
    return outHit.hit;
}
