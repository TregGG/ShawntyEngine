#pragma once
#include "uipanel.h"
#include <functional>
#include "../../services/base/eventservice.h"

class UIButton : public UIPanel {
public:
    UIButton(Scene* scene, const std::string& name, EventService* eventService);
    virtual ~UIButton();

    virtual void OnClick();
    
    std::function<void()> OnClickCallback;

    std::string ActionType;
    std::string ActionTarget;

    glm::vec4 NormalColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    glm::vec4 HoverColor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
    glm::vec4 PressedColor = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);

private:
    EventService* m_EventService;
    int m_EventSubID = -1;
    bool m_Hovered = false;
    bool m_Pressed = false;
    
    void OnEvent(Event& e);
};
