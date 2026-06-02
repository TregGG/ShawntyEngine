#include "bindings.h"

#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>

#include "../services/base/entityregistry/entityregistry.h"
#include "../objects/components/components.h"
#include "../objects/components/rigidbodycomponent.h"
#include "../objects/components/collidercomponent.h"
#include "../objects/components/animator.h"
#include "../core/input.h"
#include "../services/networkservice.h"
#include "../services/networkcontrol.h"

#include <GLFW/glfw3.h>
#include <string>
#include <cmath>

namespace py = pybind11;

static NetworkService* s_NetService = nullptr;
static NetworkControl* s_NetControl = nullptr;

std::function<void(const std::string&)> g_ChangeSceneCallback;

void SetNetworkBindings(NetworkService* ns, NetworkControl* nc) {
    s_NetService = ns;
    s_NetControl = nc;
}

// ============================================================
// The "shawnty" embedded Python module
// This is automatically available to any Python script running
// inside the engine's embedded interpreter.
// ============================================================
PYBIND11_EMBEDDED_MODULE(shawnty, m) {
    m.doc() = "ShawntyEngine Python scripting API";

    // ========================================
    // Vec2 — wraps glm::vec2
    // ========================================
    py::class_<glm::vec2>(m, "Vec2")
        .def(py::init<>())
        .def(py::init<float, float>())
        .def_readwrite("x", &glm::vec2::x)
        .def_readwrite("y", &glm::vec2::y)
        .def("__repr__", [](const glm::vec2& v) {
            return "Vec2(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
        })
        .def("__add__", [](const glm::vec2& a, const glm::vec2& b) { return a + b; })
        .def("__sub__", [](const glm::vec2& a, const glm::vec2& b) { return a - b; })
        .def("__mul__", [](const glm::vec2& a, float s) { return a * s; })
        .def("__rmul__", [](const glm::vec2& a, float s) { return s * a; })
        .def("__neg__", [](const glm::vec2& a) { return -a; })
        .def("length", [](const glm::vec2& v) { return glm::length(v); })
        .def("normalized", [](const glm::vec2& v) {
            float len = glm::length(v);
            if (len < 0.0001f) return glm::vec2(0.0f);
            return v / len;
        })
        .def_static("distance", [](const glm::vec2& a, const glm::vec2& b) {
            return glm::distance(a, b);
        })
        .def_static("dot", [](const glm::vec2& a, const glm::vec2& b) {
            return glm::dot(a, b);
        })
        .def_static("zero", []() { return glm::vec2(0.0f); })
        .def_static("one", []() { return glm::vec2(1.0f); })
        .def_static("up", []() { return glm::vec2(0.0f, 1.0f); })
        .def_static("right", []() { return glm::vec2(1.0f, 0.0f); });

    // ========================================
    // Transform — proxy for TransformComponent
    // ========================================
    py::class_<TransformProxy>(m, "Transform")
        .def_property("position",
            [](const TransformProxy& t) -> glm::vec2 {
                if (!t.registry->HasComponent<TransformComponent>(t.entityId)) return glm::vec2(0.0f);
                return t.registry->GetComponent<TransformComponent>(t.entityId).position;
            },
            [](TransformProxy& t, const glm::vec2& pos) {
                if (!t.registry->HasComponent<TransformComponent>(t.entityId)) return;
                t.registry->GetComponent<TransformComponent>(t.entityId).position = pos;
            })
        .def_property("local_position",
            [](const TransformProxy& t) -> glm::vec2 {
                if (!t.registry->HasComponent<TransformComponent>(t.entityId)) return glm::vec2(0.0f);
                return t.registry->GetComponent<TransformComponent>(t.entityId).localPosition;
            },
            [](TransformProxy& t, const glm::vec2& pos) {
                if (!t.registry->HasComponent<TransformComponent>(t.entityId)) return;
                t.registry->GetComponent<TransformComponent>(t.entityId).localPosition = pos;
            })
        .def_property("size",
            [](const TransformProxy& t) -> glm::vec2 {
                if (!t.registry->HasComponent<TransformComponent>(t.entityId)) return glm::vec2(1.0f);
                return t.registry->GetComponent<TransformComponent>(t.entityId).size;
            },
            [](TransformProxy& t, const glm::vec2& s) {
                if (!t.registry->HasComponent<TransformComponent>(t.entityId)) return;
                t.registry->GetComponent<TransformComponent>(t.entityId).size = s;
            })
        .def_property("rotation",
            [](const TransformProxy& t) -> float {
                if (!t.registry->HasComponent<TransformComponent>(t.entityId)) return 0.0f;
                return t.registry->GetComponent<TransformComponent>(t.entityId).rotation;
            },
            [](TransformProxy& t, float r) {
                if (!t.registry->HasComponent<TransformComponent>(t.entityId)) return;
                t.registry->GetComponent<TransformComponent>(t.entityId).rotation = r;
            })
        .def("get_world_position", [](const TransformProxy& t) -> glm::vec2 {
            if (!t.registry->HasComponent<TransformComponent>(t.entityId)) return glm::vec2(0.0f);
            return t.registry->GetComponent<TransformComponent>(t.entityId).GetWorldPosition();
        })
        .def("__repr__", [](const TransformProxy& t) {
            if (!t.registry->HasComponent<TransformComponent>(t.entityId)) return std::string("Transform(invalid)");
            auto& tc = t.registry->GetComponent<TransformComponent>(t.entityId);
            return "Transform(pos=" + std::to_string(tc.position.x) + "," + std::to_string(tc.position.y) +
                   " size=" + std::to_string(tc.size.x) + "," + std::to_string(tc.size.y) + ")";
        });

    // ========================================
    // RigidBody — proxy for RigidBodyComponent
    // ========================================
    py::class_<RigidBodyProxy>(m, "RigidBody")
        .def_property("velocity",
            [](const RigidBodyProxy& r) -> glm::vec2 {
                if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return glm::vec2(0.0f);
                return r.registry->GetComponent<RigidBodyComponent>(r.entityId).GetVelocity();
            },
            [](RigidBodyProxy& r, const glm::vec2& v) {
                if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return;
                r.registry->GetComponent<RigidBodyComponent>(r.entityId).SetVelocity(v);
            })
        .def("add_force", [](RigidBodyProxy& r, const glm::vec2& force) {
            if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return;
            r.registry->GetComponent<RigidBodyComponent>(r.entityId).AddForce(force);
        })
        .def("apply_impulse", [](RigidBodyProxy& r, const glm::vec2& impulse) {
            if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return;
            r.registry->GetComponent<RigidBodyComponent>(r.entityId).ApplyLinearImpulse(impulse);
        })
        .def_property("drag",
            [](const RigidBodyProxy& r) -> float {
                if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return 0.0f;
                return r.registry->GetComponent<RigidBodyComponent>(r.entityId).GetDrag();
            },
            [](RigidBodyProxy& r, float d) {
                if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return;
                r.registry->GetComponent<RigidBodyComponent>(r.entityId).SetDrag(d);
            })
        .def_property("gravity_scale",
            [](const RigidBodyProxy& r) -> float {
                if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return 1.0f;
                return r.registry->GetComponent<RigidBodyComponent>(r.entityId).GetGravityScale();
            },
            [](RigidBodyProxy& r, float s) {
                if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return;
                r.registry->GetComponent<RigidBodyComponent>(r.entityId).SetGravityScale(s);
            })
        .def_property("use_gravity",
            [](const RigidBodyProxy& r) -> bool {
                if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return false;
                return r.registry->GetComponent<RigidBodyComponent>(r.entityId).GetUseGravity();
            },
            [](RigidBodyProxy& r, bool use) {
                if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return;
                r.registry->GetComponent<RigidBodyComponent>(r.entityId).SetUseGravity(use);
            })
        .def_property("elasticity",
            [](const RigidBodyProxy& r) -> float {
                if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return 0.0f;
                return r.registry->GetComponent<RigidBodyComponent>(r.entityId).GetElasticity();
            },
            [](RigidBodyProxy& r, float e) {
                if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return;
                r.registry->GetComponent<RigidBodyComponent>(r.entityId).SetElasticity(e);
            })
        .def_property("mass",
            [](const RigidBodyProxy& r) -> float {
                if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return 1.0f;
                return r.registry->GetComponent<RigidBodyComponent>(r.entityId).GetMass();
            },
            [](RigidBodyProxy& r, float mass) {
                if (!r.registry->HasComponent<RigidBodyComponent>(r.entityId)) return;
                r.registry->GetComponent<RigidBodyComponent>(r.entityId).SetMass(mass);
            });

    // ========================================
    // Collider — proxy for ColliderComponent
    // ========================================
    py::class_<ColliderProxy>(m, "Collider")
        .def_property("is_trigger",
            [](const ColliderProxy& c) -> bool {
                if (!c.registry->HasComponent<ColliderComponent>(c.entityId)) return false;
                return c.registry->GetComponent<ColliderComponent>(c.entityId).IsTrigger();
            },
            [](ColliderProxy& c, bool trigger) {
                if (!c.registry->HasComponent<ColliderComponent>(c.entityId)) return;
                c.registry->GetComponent<ColliderComponent>(c.entityId).SetTrigger(trigger);
            })
        .def_property("layer_mask",
            [](const ColliderProxy& c) -> uint32_t {
                if (!c.registry->HasComponent<ColliderComponent>(c.entityId)) return 0xFFFFFFFF;
                return c.registry->GetComponent<ColliderComponent>(c.entityId).GetLayerMask();
            },
            [](ColliderProxy& c, uint32_t mask) {
                if (!c.registry->HasComponent<ColliderComponent>(c.entityId)) return;
                c.registry->GetComponent<ColliderComponent>(c.entityId).SetLayerMask(mask);
            });

    // ========================================
    // Animator — proxy for AnimatorComponent
    // ========================================
    py::class_<AnimatorProxy>(m, "Animator")
        .def("play", [](AnimatorProxy& a, const std::string& clipName, bool loop) {
            if (!a.registry->HasComponent<AnimatorComponent>(a.entityId)) return;
            a.registry->GetComponent<AnimatorComponent>(a.entityId).Play(clipName, loop);
        }, py::arg("clip_name"), py::arg("loop") = true)
        .def("stop", [](AnimatorProxy& a) {
            if (!a.registry->HasComponent<AnimatorComponent>(a.entityId)) return;
            a.registry->GetComponent<AnimatorComponent>(a.entityId).Stop();
        })
        .def("has_animation", [](const AnimatorProxy& a, const std::string& clipName) -> bool {
            if (!a.registry->HasComponent<AnimatorComponent>(a.entityId)) return false;
            return a.registry->GetComponent<AnimatorComponent>(a.entityId).HasAnimation(clipName);
        })
        .def_property_readonly("is_playing", [](const AnimatorProxy& a) -> bool {
            if (!a.registry->HasComponent<AnimatorComponent>(a.entityId)) return false;
            return a.registry->GetComponent<AnimatorComponent>(a.entityId).IsPlaying();
        })
        .def("set_speed", [](AnimatorProxy& a, float speed) {
            if (!a.registry->HasComponent<AnimatorComponent>(a.entityId)) return;
            a.registry->GetComponent<AnimatorComponent>(a.entityId).SetSpeed(speed);
        });

    // ========================================
    // Input — read-only input state
    // ========================================
    py::class_<InputProxy>(m, "Input")
        .def("is_key_down", [](const InputProxy& ip, int key) -> bool {
            if (!ip.input) return false;
            return ip.input->IsKeyDown(key);
        })
        .def("is_key_pressed", [](const InputProxy& ip, int key) -> bool {
            if (!ip.input) return false;
            return ip.input->IsKeyPressed(key);
        })
        .def("is_key_released", [](const InputProxy& ip, int key) -> bool {
            if (!ip.input) return false;
            return ip.input->IsKeyReleased(key);
        })
        .def("is_mouse_down", [](const InputProxy& ip, int button) -> bool {
            if (!ip.input) return false;
            return ip.input->IsMouseButtonDown(button);
        })
        .def("is_mouse_pressed", [](const InputProxy& ip, int button) -> bool {
            if (!ip.input) return false;
            return ip.input->IsMouseButtonPressed(button);
        })
        .def("get_mouse_position", [](const InputProxy& ip) -> glm::vec2 {
            if (!ip.input) return glm::vec2(0.0f);
            double x, y;
            ip.input->GetMousePosition(x, y);
            return glm::vec2(static_cast<float>(x), static_cast<float>(y));
        });

    // ========================================
    // Entity — handle to an engine entity
    // ========================================
    py::class_<EntityHandle>(m, "Entity")
        .def("get_name", [](const EntityHandle& e) -> std::string {
            if (!e.registry || !e.registry->IsAlive(e.entityId)) return "Invalid";
            return std::string(e.registry->GetName(e.entityId));
        })
        .def("get_id", [](const EntityHandle& e) -> uint64_t {
            return e.entityId;
        })
        .def("is_alive", [](const EntityHandle& e) -> bool {
            return e.registry && e.registry->IsAlive(e.entityId);
        })
        .def("get_category", [](const EntityHandle& e) -> std::string {
            if (!e.registry || !e.registry->IsAlive(e.entityId)) return "Unknown";
            EntityCategory cat = e.registry->GetCategory(e.entityId);
            if (cat == EntityCategory::Player) return "Player";
            if (cat == EntityCategory::Environment) return "Environment";
            if (cat == EntityCategory::Enemy) return "Enemy";
            if (cat == EntityCategory::Projectile) return "Projectile";
            if (cat == EntityCategory::UI) return "UI";
            return "Unknown";
        })
        .def("get_transform", [](const EntityHandle& e) -> py::object {
            if (!e.registry || !e.registry->HasComponent<TransformComponent>(e.entityId))
                return py::none();
            return py::cast(TransformProxy{e.registry, e.entityId});
        })
        .def("get_rigidbody", [](const EntityHandle& e) -> py::object {
            if (!e.registry || !e.registry->HasComponent<RigidBodyComponent>(e.entityId))
                return py::none();
            return py::cast(RigidBodyProxy{e.registry, e.entityId});
        })
        .def("get_collider", [](const EntityHandle& e) -> py::object {
            if (!e.registry || !e.registry->HasComponent<ColliderComponent>(e.entityId))
                return py::none();
            return py::cast(ColliderProxy{e.registry, e.entityId});
        })
        .def("get_animator", [](const EntityHandle& e) -> py::object {
            if (!e.registry || !e.registry->HasComponent<AnimatorComponent>(e.entityId))
                return py::none();
            return py::cast(AnimatorProxy{e.registry, e.entityId});
        })
        .def("destroy", [](EntityHandle& e) {
            if (e.registry && e.registry->IsAlive(e.entityId)) {
                e.registry->Destroy(e.entityId);
            }
        })
        .def("__repr__", [](const EntityHandle& e) -> std::string {
            if (!e.registry || !e.registry->IsAlive(e.entityId))
                return "Entity(invalid)";
            return "Entity('" + std::string(e.registry->GetName(e.entityId)) + "', id=" + std::to_string(e.entityId) + ")";
        });

    // ========================================
    // Key constants (GLFW key codes)
    // ========================================
    m.attr("KEY_W") = GLFW_KEY_W;
    m.attr("KEY_A") = GLFW_KEY_A;
    m.attr("KEY_S") = GLFW_KEY_S;
    m.attr("KEY_D") = GLFW_KEY_D;
    m.attr("KEY_Q") = GLFW_KEY_Q;
    m.attr("KEY_E") = GLFW_KEY_E;
    m.attr("KEY_R") = GLFW_KEY_R;
    m.attr("KEY_F") = GLFW_KEY_F;
    m.attr("KEY_SPACE") = GLFW_KEY_SPACE;
    m.attr("KEY_ESCAPE") = GLFW_KEY_ESCAPE;
    m.attr("KEY_ENTER") = GLFW_KEY_ENTER;
    m.attr("KEY_TAB") = GLFW_KEY_TAB;
    m.attr("KEY_SHIFT") = GLFW_KEY_LEFT_SHIFT;
    m.attr("KEY_CTRL") = GLFW_KEY_LEFT_CONTROL;
    m.attr("KEY_ALT") = GLFW_KEY_LEFT_ALT;
    m.attr("KEY_UP") = GLFW_KEY_UP;
    m.attr("KEY_DOWN") = GLFW_KEY_DOWN;
    m.attr("KEY_LEFT") = GLFW_KEY_LEFT;
    m.attr("KEY_RIGHT") = GLFW_KEY_RIGHT;
    m.attr("KEY_1") = GLFW_KEY_1;
    m.attr("KEY_2") = GLFW_KEY_2;
    m.attr("KEY_3") = GLFW_KEY_3;
    m.attr("KEY_4") = GLFW_KEY_4;
    m.attr("KEY_5") = GLFW_KEY_5;

    // Mouse buttons
    m.attr("MOUSE_LEFT") = GLFW_MOUSE_BUTTON_LEFT;
    m.attr("MOUSE_RIGHT") = GLFW_MOUSE_BUTTON_RIGHT;
    m.attr("MOUSE_MIDDLE") = GLFW_MOUSE_BUTTON_MIDDLE;

    // Global Functions
    m.def("is_server", []() -> bool {
        if (s_NetService) {
            return s_NetService->GetMode() == NetworkMode::Server;
        }
        return false;
    });

    m.def("get_active_players", [](const EntityHandle& e) -> py::list {
        py::list result;
        if (s_NetControl && e.registry) {
            std::vector<EntityID> players = s_NetControl->GetActivePlayerEntities();
            for (EntityID id : players) {
                if (e.registry->IsAlive(id)) {
                    result.append(EntityHandle{e.registry, id});
                }
            }
        }
        return result;
    });

    m.def("change_scene", [](const std::string& scenePath) {
        if (g_ChangeSceneCallback) {
            g_ChangeSceneCallback(scenePath);
        }
    });
}
