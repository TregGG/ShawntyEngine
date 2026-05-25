#pragma once
#include "uiobject.h"
#include <string>

class UIImage : public UIObject {
public:
    UIImage(Scene* scene, const std::string& name, unsigned int textureID)
        : UIObject(scene, name)
    {
        BackgroundTexture = textureID;
        BackgroundColor = glm::vec4(1.0f); // White tint for texture rendering
    }
    
    virtual ~UIImage() = default;
};
