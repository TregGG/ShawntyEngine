#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "../gameobject.h"

class UIObject : public GameObject {
public:
    UIObject(Scene* scene, const std::string& name);
    virtual ~UIObject() = default;

    virtual void Update(float dt);
    virtual void Render(const glm::mat4& projection);

    void AddChild(std::unique_ptr<UIObject> child);
    
    glm::vec2 Position = glm::vec2(0.0f);
    glm::vec2 Size = glm::vec2(100.0f, 100.0f);
    glm::vec4 BackgroundColor = glm::vec4(0.0f); // r,g,b,a (0 alpha = transparent)
    unsigned int BackgroundTexture = 0;

    UIObject* GetParent() const { return m_Parent; }

protected:
    UIObject* m_Parent = nullptr;
    std::vector<std::unique_ptr<UIObject>> m_Children;
    
    void RenderBackground(const glm::mat4& projection);
};
