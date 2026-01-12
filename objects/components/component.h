#pragma once

#include <string>

class GameObject;



class Component
{
public:
    Component()=default;
    virtual ~Component() = default;

    // ----- Owner Functions -----
    void SetOwner(GameObject* object)
    {
        m_Owner=object;
    }
    GameObject* GetOwner() { return m_Owner; }
    const GameObject* GetOwner() const {return m_Owner;}

    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    bool IsEnabled() const { return m_Active; }
    void SetEnabled(bool active) { m_Active = active; }

    virtual void Update(float deltatime)=0;

protected:
    GameObject* m_Owner=nullptr;
    bool m_Active=true;
    std::string m_Name;

};
