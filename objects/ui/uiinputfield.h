#pragma once
#include "uipanel.h"
#include "../../services/base/eventservice.h"

class UIText;
class FontEngine;

class UIInputField : public UIPanel {
public:
    UIInputField(Scene* scene, const std::string& name, EventService* eventService, FontEngine* fontEngine);
    virtual ~UIInputField();

    virtual void Update(float dt) override;
    
    UIText* GetTextElement() const { return m_TextElement; }

private:
    EventService* m_EventService;
    int m_EventSubID = -1;
    bool m_Focused = false;
    
    UIText* m_TextElement = nullptr; // weak ptr to child
    
    void OnEvent(Event& e);
};
