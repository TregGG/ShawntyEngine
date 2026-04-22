#pragma once
#include "component.h"
#include <glm/vec2.hpp>

enum class BodyType {
    Static = 0,
    Kinematic,
    Dynamic
};

class RigidBodyComponent : public Component {
public:
    RigidBodyComponent();
    virtual ~RigidBodyComponent() = default;

    void Update(float /*dt*/) override {}

    BodyType GetType() const { return m_Type; }
    void SetType(BodyType type) { m_Type = type; }

    const glm::vec2& GetVelocity() const { return m_Velocity; }
    void SetVelocity(const glm::vec2& vel) { m_Velocity = vel; }

    const glm::vec2& GetAcceleration() const { return m_Acceleration; }
    void AddForce(const glm::vec2& force);

    float GetMass() const { return m_Mass; }
    float GetInverseMass() const { return m_InvMass; }
    void SetMass(float mass);

    float GetDrag() const { return m_Drag; }
    void SetDrag(float drag) { m_Drag = drag; }

    float GetGravityScale() const { return m_GravityScale; }
    void SetGravityScale(float scale) { m_GravityScale = scale; }

    bool GetUseGravity() const { return m_UseGravity; }
    void SetUseGravity(bool use) { m_UseGravity = use; }

    void ClearForces() { m_Acceleration = glm::vec2(0.0f); }

private:
    BodyType m_Type = BodyType::Dynamic;
    glm::vec2 m_Velocity { 0.0f };
    glm::vec2 m_Acceleration { 0.0f };
    
    float m_Mass = 1.0f;
    float m_InvMass = 1.0f;
    float m_Drag = 0.0f; // Linear damping basically
    float m_GravityScale = 1.0f;
    bool m_UseGravity = false; // Default off for top-down, let user turn on explicitly
};
