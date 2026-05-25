#pragma once
#include "uiobject.h"
#include "../../render/fontengine.h"

enum class TextAlignment { Left, Center, Right };
enum class VerticalAlignment { Top, Middle, Bottom };

class UIText : public UIObject {
public:
    UIText(Scene* scene, const std::string& name, FontEngine* fontEngine);
    
    virtual void Render(const glm::mat4& projection) override;
    
    std::string Text;
    glm::vec3 TextColor = glm::vec3(1.0f);
    float Scale = 1.0f;
    TextAlignment HorizontalAlign = TextAlignment::Center;
    VerticalAlignment VerticalAlign = VerticalAlignment::Middle;
    
private:
    FontEngine* m_FontEngine;
};
