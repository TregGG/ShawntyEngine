#pragma once
#include <string>
#include <glm/vec2.hpp>
#include <vector>
#include <memory>
#include "components/component.h"


struct Transform2D
{
    glm::vec2 position {0.0f, 0.0f};
    glm::vec2 size     {1.0f, 1.0f};
    float rotation     = 0.0f;
};


class GameObject
{
public:
    virtual ~GameObject() = default;
    explicit GameObject(const std::string& name)
        : m_Name(name)
    {}

    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    bool IsActive() const { return m_Active; }
    void SetActive(bool active) { m_Active = active; }

    Transform2D& GetTransform() { return m_Transform; }
    const Transform2D& GetTransform() const { return m_Transform; }

    virtual void Update(float deltaTime)
    {
        if(!m_Active) return;
        UpdateComponents(deltaTime);
    }

    void AddComponent(std::unique_ptr<Component> component)
    {
        component->SetOwner(this);
        m_Components.emplace_back(std::move(component));
    }

    // Get a component by type
    template<typename T>
    T* GetComponent()
    {
        for (auto& comp : m_Components)
        {
            T* typedComp = dynamic_cast<T*>(comp.get());
            if (typedComp)
                return typedComp;
        }
        return nullptr;
    }

protected:
    void UpdateComponents(float deltaTime)
    {
         for (auto& comp : m_Components)
        {
            if (comp->IsEnabled())
                comp->Update(deltaTime);
        }
    }
protected:
    std::vector<std::unique_ptr<Component>> m_Components;
    std::string m_Name;
    bool m_Active = true;
    Transform2D m_Transform;

};
