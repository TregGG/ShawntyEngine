#include "uitext.h"

UIText::UIText(Scene* scene, const std::string& name, FontEngine* fontEngine)
    : UIObject(scene, name), m_FontEngine(fontEngine) {}

void UIText::Render(const glm::mat4& projection)
{
    // Draw background (if any) first
    RenderBackground(projection);
    
    // Draw text
    if (m_FontEngine && !Text.empty())
    {
        // Calculate absolute position
        glm::vec2 absPos = Position;
        UIObject* currParent = m_Parent;
        while (currParent) {
            absPos += currParent->Position;
            currParent = currParent->GetParent();
        }

        glm::vec2 boundsSize = Size;
        if (boundsSize.x == 0 && boundsSize.y == 0 && m_Parent) {
            boundsSize = m_Parent->Size;
        }

        glm::vec2 textSize = m_FontEngine->MeasureText(Text, Scale);
        float standardHeight = m_FontEngine->GetLineHeight(Scale);
        float standardAscender = m_FontEngine->GetStandardAscender(Scale);

        float drawX = absPos.x;
        if (HorizontalAlign == TextAlignment::Center) {
            drawX += (boundsSize.x - textSize.x) / 2.0f;
        } else if (HorizontalAlign == TextAlignment::Right) {
            drawX += boundsSize.x - textSize.x;
        }

        float drawY = absPos.y;
        if (VerticalAlign == VerticalAlignment::Middle) {
            drawY += (boundsSize.y - standardHeight) / 2.0f + standardAscender;
        } else if (VerticalAlign == VerticalAlignment::Bottom) {
            drawY += boundsSize.y - (standardHeight - standardAscender);
        } else {
            // Top align
            drawY += standardAscender;
        }

        m_FontEngine->RenderText(Text, drawX, drawY, Scale, TextColor, projection);
    }
    
    // Draw children
    for (auto& child : m_Children) {
        child->Render(projection);
    }
}
