#pragma once
#include "../core/entityid.h"
#include <glm/vec2.hpp>

class EntityRegistryService;
class Input;

// Lightweight proxy objects that hold registry + entityId references.
// Scripts interact with these instead of raw C++ component pointers.
// Each proxy checks entity liveness before access, preventing crashes
// if an entity is destroyed while a script still references it.

struct EntityHandle {
    EntityRegistryService* registry = nullptr;
    EntityID entityId = 0;
};

struct TransformProxy {
    EntityRegistryService* registry = nullptr;
    EntityID entityId = 0;
};

struct RigidBodyProxy {
    EntityRegistryService* registry = nullptr;
    EntityID entityId = 0;
};

struct ColliderProxy {
    EntityRegistryService* registry = nullptr;
    EntityID entityId = 0;
};

struct AnimatorProxy {
    EntityRegistryService* registry = nullptr;
    EntityID entityId = 0;
};

struct InputProxy {
    const Input* input = nullptr;
};

#include <functional>
#include <string>

class NetworkService;
class NetworkControl;

void SetNetworkBindings(NetworkService* ns, NetworkControl* nc);
extern std::function<void(const std::string&)> g_ChangeSceneCallback;
