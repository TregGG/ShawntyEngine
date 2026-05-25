#pragma once
#include "uiobject.h"

// UIPanel is essentially just a UIObject that renders its background.
// We provide it as a separate class for semantic meaning and future expansion.
class UIPanel : public UIObject {
public:
    UIPanel(Scene* scene, const std::string& name) : UIObject(scene, name) {}
    virtual ~UIPanel() = default;
};
