#include "uibutton.h"

UIButton::UIButton(Scene* scene, const std::string& name, EventService* eventService)
    : UIPanel(scene, name), m_EventService(eventService)
{
    BackgroundColor = NormalColor;
    if (m_EventService) {
        m_EventSubID = m_EventService->Subscribe([this](Event& e) { this->OnEvent(e); });
    }
}

UIButton::~UIButton()
{
    if (m_EventService && m_EventSubID != -1) {
        m_EventService->Unsubscribe(m_EventSubID);
    }
}

void UIButton::OnClick()
{
    if (OnClickCallback) {
        OnClickCallback();
    }
    if (m_Scene && !ActionType.empty()) {
        m_Scene->registry.TriggerUIAction(ActionType, ActionTarget);
    }
}

void UIButton::OnEvent(Event& e)
{
    glm::vec2 absPos = Position;
    UIObject* currParent = m_Parent;
    while (currParent) {
        absPos += currParent->Position;
        currParent = currParent->GetParent();
    }

    if (e.type == EventType::MouseMoved) {
        auto& me = static_cast<MouseEvent&>(e);
        bool inBounds = (me.x >= absPos.x && me.x <= absPos.x + Size.x &&
                         me.y >= absPos.y && me.y <= absPos.y + Size.y);
        
        if (inBounds && !m_Hovered) {
            m_Hovered = true;
            if (!m_Pressed) BackgroundColor = HoverColor;
        } else if (!inBounds && m_Hovered) {
            m_Hovered = false;
            m_Pressed = false;
            BackgroundColor = NormalColor;
        }
    }
    else if (e.type == EventType::MousePressed) {
        auto& me = static_cast<MouseEvent&>(e);
        if (me.button == 0 && m_Hovered) { // Left click
            m_Pressed = true;
            BackgroundColor = PressedColor;
            e.handled = true; // Consume event!
        }
    }
    else if (e.type == EventType::MouseReleased) {
        auto& me = static_cast<MouseEvent&>(e);
        if (me.button == 0 && m_Pressed) {
            m_Pressed = false;
            if (m_Hovered) {
                BackgroundColor = HoverColor;
                OnClick();
            } else {
                BackgroundColor = NormalColor;
            }
            e.handled = true;
        }
    }
}
