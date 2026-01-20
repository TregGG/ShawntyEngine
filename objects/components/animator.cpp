#include "animator_component.h"
#include "../../assets/assetdatastruct.h"

void AnimatorComponent::BindAnimationSet(const AnimationSetAsset* animSet,
                                        const SpriteSheetAsset* spriteSheet)
{
    m_AnimSet     = animSet;
    m_SpriteSheet = spriteSheet;

    // Reset state
    m_CurrentClip   = nullptr;
    m_ClipFrameIdx  = 0;
    m_TimeInFrame   = 0.0f;
    m_Playing       = false;
}

bool AnimatorComponent::HasAnimation(const std::string& clipName) const
{
    if (!m_AnimSet)
        return false;

    return m_AnimSet->clips.find(clipName) != m_AnimSet->clips.end();
}

void AnimatorComponent::Play(const std::string& clipNam3e, bool loop)
{
    if (!m_AnimSet)
        return;

    auto it = m_AnimSet->clips.find(clipName);
    if (it == m_AnimSet->clips.end())
        return;

    m_CurrentClip  = &it->second;
    m_Loop         = loop;
    m_Playing      = true;

    m_ClipFrameIdx = 0;
    m_TimeInFrame  = 0.0f;
}

void AnimatorComponent::Stop()
{
    m_Playing      = false;
    m_ClipFrameIdx = 0;
    m_TimeInFrame  = 0.0f;
}

void AnimatorComponent::Update(float deltaTime)
{
    if (!m_Enabled || !m_Playing || !m_CurrentClip)
        return;

    m_TimeInFrame += deltaTime * m_Speed;

    const auto& frame = m_CurrentClip->frames[m_ClipFrameIdx];

    if (m_TimeInFrame >= frame.duration)
    {
        m_TimeInFrame -= frame.duration;
        ++m_ClipFrameIdx;

        if (m_ClipFrameIdx >= m_CurrentClip->frames.size())
        {
            if (m_Loop)
            {
                m_ClipFrameIdx = 0;
            }
            else
            {
                m_ClipFrameIdx = m_CurrentClip->frames.size() - 1;
                m_Playing = false;
            }
        }
    }
}

int AnimatorComponent::GetFrameIndex() const
{
    if (!m_CurrentClip)
        return 0;

    return m_CurrentClip->frames[m_ClipFrameIdx].frameIndex;
}

const SpriteSheetAsset* AnimatorComponent::GetSpriteSheet() const
{
    return m_SpriteSheet;
}
