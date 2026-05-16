#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "../../core/entityid.h"

// Forward declaration
struct SpriteSheetAsset;

struct TransformComponent {
    glm::vec2 localPosition = {0.0f, 0.0f};
    glm::vec2 position = {0.0f, 0.0f}; // World position (computed or set)
    glm::vec2 size = {1.0f, 1.0f};
    float rotation = 0.0f;
};

struct SpriteComponent2D {
    const SpriteSheetAsset* spriteSheet = nullptr;
    int frameIndex = 0;
    Layer layer = Layer::Foreground;
};

struct RelationshipComponent {
    EntityID parent = 0;
    std::vector<EntityID> children;
};
