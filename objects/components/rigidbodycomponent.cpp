#include "rigidbodycomponent.h"

RigidBodyComponent::RigidBodyComponent() {}

void RigidBodyComponent::AddForce(const glm::vec2& force) {
    if (m_Type == BodyType::Dynamic && m_InvMass > 0.0f) {
        m_Acceleration += force * m_InvMass;
    }
}

void RigidBodyComponent::SetMass(float mass) {
    if (mass <= 0.0f) {
        m_Mass = 0.0f;
        m_InvMass = 0.0f;
        if (m_Type == BodyType::Dynamic) m_Type = BodyType::Static;
    } else {
        m_Mass = mass;
        m_InvMass = 1.0f / mass;
    }
}
