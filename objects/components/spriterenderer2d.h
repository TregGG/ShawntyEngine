#pragma once

#include "component.h"

struct SpriteSheetAsset;

class SpriteRenderer2D : public Component
{
public:
    SpriteRenderer2D() = default;
    virtual ~SpriteRenderer2D() = default;

    // Set the sprite sheet for this renderer
    void SetSpriteSheet(const SpriteSheetAsset* sheet)
    {
        m_SpriteSheet = sheet;
    }

    const SpriteSheetAsset* GetSpriteSheet() const
    {
        return m_SpriteSheet;
    }

    // Set current frame index (used by scene or animator)
    void SetFrameIndex(int frame)
    {
        m_FrameIndex = frame;
    }

    int GetFrameIndex() const
    {
        return m_FrameIndex;
    }

    void Update(float deltatime) override
    {
        // SpriteRenderer2D is passive; Animator drives frame updates
    }

private:
    const SpriteSheetAsset* m_SpriteSheet = nullptr;
    int m_FrameIndex = 0;
};
