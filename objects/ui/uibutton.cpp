#include "uibutton.h"
#include "../../levels/scene.h"

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

    auto mapMouseCoords = [this](double& mx, double& my) {
        if (!m_Scene) return;
        float vw = m_Scene->GetViewportWidth();
        float vh = m_Scene->GetViewportHeight();
        if (vw <= 0 || vh <= 0) return;
        
        float refWidth = 1280.0f;
        float refHeight = 720.0f;
        float scaleX = vw / refWidth;
        float scaleY = vh / refHeight;
        float scale = std::min(scaleX, scaleY);
        if (scale > 1.0f) scale = 1.0f;
        
        float actualWidth = refWidth * scale;
        float actualHeight = refHeight * scale;
        float offsetX = (vw - actualWidth) / 2.0f;
        float offsetY = (vh - actualHeight) / 2.0f;
        
        mx = (mx - offsetX) / scale;
        my = (my - offsetY) / scale;
    };

    if (e.type == EventType::MouseMoved) {
        auto& me = static_cast<MouseEvent&>(e);
        double mx = me.x, my = me.y;
        mapMouseCoords(mx, my);
        bool inBounds = (mx >= absPos.x && mx <= absPos.x + Size.x &&
                         my >= absPos.y && my <= absPos.y + Size.y);
        
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
        double mx = me.x, my = me.y;
        mapMouseCoords(mx, my);
        bool inBounds = (mx >= absPos.x && mx <= absPos.x + Size.x &&
                         my >= absPos.y && my <= absPos.y + Size.y);

        if (me.button == 0 && inBounds) { // Left click
            m_Pressed = true;
            BackgroundColor = PressedColor;
            e.handled = true; // Consume event!
        }
    }
    else if (e.type == EventType::MouseReleased) {
        auto& me = static_cast<MouseEvent&>(e);
        double mx = me.x, my = me.y;
        mapMouseCoords(mx, my);
        bool inBounds = (mx >= absPos.x && mx <= absPos.x + Size.x &&
                         my >= absPos.y && my <= absPos.y + Size.y);

        if (me.button == 0 && m_Pressed) {
            m_Pressed = false;
            if (inBounds) {
                BackgroundColor = HoverColor;
                OnClick();
            } else {
                BackgroundColor = NormalColor;
            }
            e.handled = true;
        }
    }
}
