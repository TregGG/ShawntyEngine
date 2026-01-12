#include "animator_component.h"
#include "assets/data/asset_types.h"

void AnimatorComponent::Play(const AnimationClip* clip,
                             const SpriteSheetAsset* sheet,
                             bool loop)
{
    if (!clip || !sheet || clip->frames.empty())
        return;

    m_CurrentClip     = clip;
    m_SpriteSheet     = sheet;
    m_Loop            = loop;

    m_ClipFrameIndex  = 0;
    m_TimeInFrame     = 0.0f;
    m_Playing         = true;
}

void AnimatorComponent::Stop()
{
    m_Playing = false;
    m_ClipFrameIndex = 0;
    m_TimeInFrame = 0.0f;
}

void AnimatorComponent::Update(float deltaTime)
{
    if (!m_Enabled || !m_Playing || !m_CurrentClip)
        return;

    m_TimeInFrame += deltaTime * m_Speed;

    const auto& frame =
        m_CurrentClip->frames[m_ClipFrameIndex];

    if (m_TimeInFrame >= frame.duration)
    {
        m_TimeInFrame -= frame.duration;
        m_ClipFrameIndex++;

        if (m_ClipFrameIndex >= m_CurrentClip->frames.size())
        {
            if (m_Loop)
            {
                m_ClipFrameIndex = 0;
            }
            else
            {
                m_ClipFrameIndex = m_CurrentClip->frames.size() - 1;
                m_Playing = false;
            }
        }
    }
}

int AnimatorComponent::GetFrameIndex() const
{
    if (!m_CurrentClip)
        return 0;

    return m_CurrentClip->frames[m_ClipFrameIndex].frameIndex;
}

const SpriteSheetAsset* AnimatorComponent::GetSpriteSheet() const
{
    return m_SpriteSheet;
}
