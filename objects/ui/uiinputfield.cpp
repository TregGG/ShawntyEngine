#include "uiinputfield.h"
#include "uitext.h"
#include <GLFW/glfw3.h> // For keycodes

UIInputField::UIInputField(Scene* scene, const std::string& name, EventService* eventService, FontEngine* fontEngine)
    : UIPanel(scene, name), m_EventService(eventService)
{
    BackgroundColor = glm::vec4(1.0f); // White background
    
    auto textNode = std::make_unique<UIText>(scene, name + "_text", fontEngine);
    m_TextElement = textNode.get();
    m_TextElement->TextColor = glm::vec3(0.0f); // Black text
    m_TextElement->Position = glm::vec2(5.0f, 5.0f); // slight padding
    AddChild(std::move(textNode));
    
    if (m_EventService) {
        m_EventSubID = m_EventService->Subscribe([this](Event& e) { this->OnEvent(e); });
    }
}

UIInputField::~UIInputField()
{
    if (m_EventService && m_EventSubID != -1) {
        m_EventService->Unsubscribe(m_EventSubID);
    }
}

void UIInputField::Update(float dt)
{
    UIPanel::Update(dt);
    // Cursor blinking logic could go here
}

void UIInputField::OnEvent(Event& e)
{
    glm::vec2 absPos = Position;
    UIObject* currParent = m_Parent;
    while (currParent) {
        absPos += currParent->Position;
        currParent = currParent->GetParent();
    }

    if (e.type == EventType::MousePressed) {
        auto& me = static_cast<MouseEvent&>(e);
        bool inBounds = (me.x >= absPos.x && me.x <= absPos.x + Size.x &&
                         me.y >= absPos.y && me.y <= absPos.y + Size.y);
        
        if (inBounds) {
            m_Focused = true;
            e.handled = true;
        } else {
            m_Focused = false;
        }
    }
    else if (e.type == EventType::CharTyped && m_Focused) {
        auto& ce = static_cast<CharEvent&>(e);
        // Only accept printable ASCII
        if (ce.codepoint >= 32 && ce.codepoint < 127) {
            m_TextElement->Text += static_cast<char>(ce.codepoint);
            e.handled = true;
        }
    }
    else if (e.type == EventType::KeyPressed && m_Focused) {
        auto& ke = static_cast<KeyEvent&>(e);
        if (ke.key == GLFW_KEY_BACKSPACE) {
            if (!m_TextElement->Text.empty()) {
                m_TextElement->Text.pop_back();
            }
            e.handled = true;
        }
    }
}
