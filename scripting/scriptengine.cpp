#include "scriptengine.h"
#include "bindings.h"

#include <pybind11/pybind11.h>
#include <pybind11/embed.h>

#include "../objects/components/scriptcomponent.h"
#include "../services/base/entityregistry/entityregistry.h"
#include "../core/input.h"

#include <algorithm>
#include <filesystem>

#define ENGINE_CLASS "ScriptEngine"
#include "../core/enginedebug.h"

namespace py = pybind11;

// ============================================================
// PIMPL Implementation — hides pybind11 types from the header
// ============================================================
struct __attribute__((visibility("hidden"))) ScriptInstance {
    py::object pyInstance;   // The Python class instance
    bool started = false;    // Has OnStart been called?
    EntityID entityId = 0;
    std::string scriptPath;
    std::string className;
};

struct __attribute__((visibility("hidden"))) ScriptEngineImpl {
    std::unordered_map<EntityID, ScriptInstance> scripts;
};

// Global interpreter guard: Python is initialized once per process
static bool s_PythonInitialized = false;

// ============================================================
// Constructor / Destructor
// ============================================================
ScriptEngine::ScriptEngine()
    : m_Impl(new ScriptEngineImpl())
{
}

ScriptEngine::~ScriptEngine()
{
    if (m_Initialized) {
        Shutdown();
    }
    delete m_Impl;
}

// ============================================================
// Init — starts the Python interpreter (once per process)
// ============================================================
void ScriptEngine::Init()
{
    if (!s_PythonInitialized) {
        try {
            py::initialize_interpreter();
            s_PythonInitialized = true;
            ENGINE_LOG("Python interpreter initialized (Python %s)", Py_GetVersion());
        } catch (const std::exception& e) {
            ENGINE_ERROR("Failed to initialize Python: %s", e.what());
            return;
        }
    }

    m_Initialized = true;
    ENGINE_LOG("ScriptEngine initialized");
}

// ============================================================
// Shutdown — detaches all scripts (interpreter stays alive)
// ============================================================
void ScriptEngine::Shutdown()
{
    DetachAll();
    m_Initialized = false;
    ENGINE_LOG("ScriptEngine shut down");
}

// ============================================================
// AttachScript — loads a .py file, instantiates the class,
//                sets properties from JSON, stores the instance
// ============================================================
void ScriptEngine::AttachScript(EntityID entity, const ScriptComponent& scriptComp)
{
    if (!m_Initialized) return;
    if (scriptComp.scriptPath.empty() || scriptComp.className.empty()) {
        ENGINE_WARN("AttachScript: empty scriptPath or className for entity %u", entity);
        return;
    }

    // Check if already attached
    if (m_Impl->scripts.find(entity) != m_Impl->scripts.end()) {
        ENGINE_WARN("AttachScript: entity %u already has a script attached", entity);
        return;
    }

    try {
        // Use importlib.util to load module from file path
        py::module_ importlib_util = py::module_::import("importlib.util");

        // Create a unique module name from the file path
        std::string moduleName = scriptComp.scriptPath;
        // Remove .py extension
        if (moduleName.size() > 3 && moduleName.substr(moduleName.size() - 3) == ".py") {
            moduleName = moduleName.substr(0, moduleName.size() - 3);
        }
        // Replace path separators with underscores for module name
        std::replace(moduleName.begin(), moduleName.end(), '/', '_');
        std::replace(moduleName.begin(), moduleName.end(), '\\', '_');

        // Make module name unique per entity to allow multiple instances of same script
        moduleName += "_ent" + std::to_string(entity);

        // Resolve to absolute path for importlib
        std::string absPath = scriptComp.scriptPath;
        if (!std::filesystem::path(absPath).is_absolute()) {
            absPath = std::filesystem::absolute(absPath).string();
        }

        // Check file exists
        if (!std::filesystem::exists(absPath)) {
            ENGINE_ERROR("Script file not found: %s", absPath.c_str());
            return;
        }

        // Load the module
        py::object spec = importlib_util.attr("spec_from_file_location")(moduleName, absPath);
        if (spec.is_none()) {
            ENGINE_ERROR("Failed to create module spec for: %s", absPath.c_str());
            return;
        }

        py::object module = importlib_util.attr("module_from_spec")(spec);
        spec.attr("loader").attr("exec_module")(module);

        // Get the class
        if (!py::hasattr(module, scriptComp.className.c_str())) {
            ENGINE_ERROR("Class '%s' not found in script '%s'",
                         scriptComp.className.c_str(), scriptComp.scriptPath.c_str());
            return;
        }

        py::object cls = module.attr(scriptComp.className.c_str());

        // Create instance
        py::object instance = cls();

        // Set properties from the scene JSON (key-value pairs)
        if (!scriptComp.properties.empty()) {
            py::module_ json_mod = py::module_::import("json");
            for (const auto& [key, value] : scriptComp.properties) {
                try {
                    // Try to parse as JSON (handles numbers, bools, strings, etc.)
                    py::object parsed = json_mod.attr("loads")(value);
                    py::setattr(instance, key.c_str(), parsed);
                } catch (...) {
                    // If JSON parsing fails, set as raw string
                    py::setattr(instance, key.c_str(), py::cast(value));
                }
            }
        }

        // Store the script instance
        ScriptInstance si;
        si.pyInstance = std::move(instance);
        si.started = false;
        si.entityId = entity;
        si.scriptPath = scriptComp.scriptPath;
        si.className = scriptComp.className;
        m_Impl->scripts[entity] = std::move(si);

        ENGINE_LOG("Script attached: %s.%s -> entity %u",
                   scriptComp.scriptPath.c_str(), scriptComp.className.c_str(), entity);

    } catch (py::error_already_set& e) {
        ENGINE_ERROR("Python error loading script '%s': %s",
                     scriptComp.scriptPath.c_str(), e.what());
    } catch (const std::exception& e) {
        ENGINE_ERROR("Error loading script '%s': %s",
                     scriptComp.scriptPath.c_str(), e.what());
    }
}

// ============================================================
// DetachScript / DetachAll
// ============================================================
void ScriptEngine::DetachScript(EntityID entity)
{
    auto it = m_Impl->scripts.find(entity);
    if (it != m_Impl->scripts.end()) {
        // Call OnDestroy if defined
        try {
            if (py::hasattr(it->second.pyInstance, "OnDestroy") && m_Registry) {
                EntityHandle handle{m_Registry, entity};
                it->second.pyInstance.attr("OnDestroy")(handle);
            }
        } catch (py::error_already_set& e) {
            ENGINE_ERROR("Python error in OnDestroy for entity %u: %s", entity, e.what());
        }
        m_Impl->scripts.erase(it);
    }
}

void ScriptEngine::DetachAll()
{
    // Call OnDestroy on all scripts
    for (auto& [entityId, si] : m_Impl->scripts) {
        try {
            if (si.started && py::hasattr(si.pyInstance, "OnDestroy") && m_Registry) {
                EntityHandle handle{m_Registry, entityId};
                si.pyInstance.attr("OnDestroy")(handle);
            }
        } catch (py::error_already_set& e) {
            ENGINE_ERROR("Python error in OnDestroy for entity %u: %s", entityId, e.what());
        }
    }
    m_Impl->scripts.clear();
}

// ============================================================
// StartPendingScripts — calls OnStart on newly attached scripts
// ============================================================
void ScriptEngine::StartPendingScripts()
{
    if (!m_Initialized || !m_Registry) return;

    for (auto& [entityId, si] : m_Impl->scripts) {
        if (si.started) continue;
        if (!m_Registry->IsAlive(entityId)) continue;

        si.started = true;

        if (py::hasattr(si.pyInstance, "OnStart")) {
            try {
                EntityHandle handle{m_Registry, entityId};
                InputProxy inputProxy{m_Input};
                si.pyInstance.attr("OnStart")(handle, inputProxy);
            } catch (py::error_already_set& e) {
                ENGINE_ERROR("[%s.%s] OnStart error: %s",
                             si.scriptPath.c_str(), si.className.c_str(), e.what());
            }
        }
    }
}

// ============================================================
// UpdateAll — calls OnUpdate(entity, dt, input) each frame
// ============================================================
void ScriptEngine::UpdateAll(float deltaTime)
{
    if (!m_Initialized || !m_Registry) return;

    InputProxy inputProxy{m_Input};

    for (auto& [entityId, si] : m_Impl->scripts) {
        if (!si.started) continue;
        if (!m_Registry->IsAlive(entityId)) continue;

        if (py::hasattr(si.pyInstance, "OnUpdate")) {
            try {
                EntityHandle handle{m_Registry, entityId};
                si.pyInstance.attr("OnUpdate")(handle, deltaTime, inputProxy);
            } catch (py::error_already_set& e) {
                ENGINE_ERROR("[%s.%s] OnUpdate error: %s",
                             si.scriptPath.c_str(), si.className.c_str(), e.what());
            }
        }
    }
}

// ============================================================
// Trigger callbacks — called from physics dispatch
// ============================================================
void ScriptEngine::CallOnTriggerEnter(EntityID self, EntityID other)
{
    if (!m_Initialized || !m_Registry) return;

    auto it = m_Impl->scripts.find(self);
    if (it == m_Impl->scripts.end()) return;
    if (!it->second.started) return;

    if (py::hasattr(it->second.pyInstance, "OnTriggerEnter")) {
        try {
            EntityHandle selfHandle{m_Registry, self};
            EntityHandle otherHandle{m_Registry, other};
            InputProxy inputProxy{m_Input};
            it->second.pyInstance.attr("OnTriggerEnter")(selfHandle, otherHandle, inputProxy);
        } catch (py::error_already_set& e) {
            ENGINE_ERROR("[%s.%s] OnTriggerEnter error: %s",
                         it->second.scriptPath.c_str(), it->second.className.c_str(), e.what());
        }
    }
}

void ScriptEngine::CallOnTriggerExit(EntityID self, EntityID other)
{
    if (!m_Initialized || !m_Registry) return;

    auto it = m_Impl->scripts.find(self);
    if (it == m_Impl->scripts.end()) return;
    if (!it->second.started) return;

    if (py::hasattr(it->second.pyInstance, "OnTriggerExit")) {
        try {
            EntityHandle selfHandle{m_Registry, self};
            EntityHandle otherHandle{m_Registry, other};
            InputProxy inputProxy{m_Input};
            it->second.pyInstance.attr("OnTriggerExit")(selfHandle, otherHandle, inputProxy);
        } catch (py::error_already_set& e) {
            ENGINE_ERROR("[%s.%s] OnTriggerExit error: %s",
                         it->second.scriptPath.c_str(), it->second.className.c_str(), e.what());
        }
    }
}

// ============================================================
// Query
// ============================================================
bool ScriptEngine::HasScript(EntityID entity) const
{
    return m_Impl->scripts.find(entity) != m_Impl->scripts.end();
}
