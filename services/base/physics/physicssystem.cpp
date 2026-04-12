#include "physicssystem.h"
#define ENGINE_CLASS "PhysicsSystem"
#include "../../../core/enginedebug.h"
#include "../../../objects/components/collidercomponent.h"
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

void PhysicsSystem::Update(float /*dt*/) {
    CheckCollisions();
}

void PhysicsSystem::Shutdown() {
    if (m_Quadtree) m_Quadtree->Clear();
    m_Colliders.clear();
    m_Collisions.clear();
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

bool PhysicsSystem::Intersects(ColliderComponent* a, ColliderComponent* b) {
    auto boundsA = a->GetBounds();
    auto boundsB = b->GetBounds();

    return !(boundsA.maxX < boundsB.minX || 
             boundsA.minX > boundsB.maxX || 
             boundsA.maxY < boundsB.minY || 
             boundsA.minY > boundsB.maxY);
}

void PhysicsSystem::CheckCollisions() {
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
                if (Intersects(m_Colliders[i], potentialCollisions[j])) {
                    m_Collisions.push_back({m_Colliders[i], potentialCollisions[j]});
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

const std::vector<CollisionEvent>& PhysicsSystem::GetCollisions() const {
    return m_Collisions;
}
