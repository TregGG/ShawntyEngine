#pragma once

#include <string>
#include <unordered_map>
#include "../core/entityid.h"

// Forward declarations — avoid pybind11 in header via PIMPL
struct ScriptEngineImpl;
struct ScriptComponent;
class EntityRegistryService;
class Input;

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    // Lifecycle
    void Init();
    void Shutdown();

    // Configuration
    void BindRegistry(EntityRegistryService* registry) { m_Registry = registry; }
    void SetInput(const Input* input) { m_Input = input; }

    // Script management
    void AttachScript(EntityID entity, const ScriptComponent& scriptComp);
    void DetachScript(EntityID entity);
    void DetachAll();

    // Frame callbacks — called by DataDrivenScene
    void StartPendingScripts();
    void UpdateAll(float deltaTime);

    // Trigger callbacks — called by physics trigger dispatch
    void CallOnTriggerEnter(EntityID self, EntityID other);
    void CallOnTriggerExit(EntityID self, EntityID other);

    // Query
    bool HasScript(EntityID entity) const;
    bool IsInitialized() const { return m_Initialized; }

private:
    ScriptEngineImpl* m_Impl = nullptr;
    EntityRegistryService* m_Registry = nullptr;
    const Input* m_Input = nullptr;
    bool m_Initialized = false;
};
