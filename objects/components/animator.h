#pragma once

#include <cstddef>
#include "component.h"

struct AnimationClip;
struct SpriteSheetAsset;

class Animator : public Component
{
public:
    Animator() = default;
     virtual ~AnimatorComponent() = default;
    // ---- Control ----
    void Play(const AnimationClip* clip,
              const SpriteSheetAsset* sheet,
              bool loop = true);

    void Stop();

    // ---- Update ----
    virtual void Update(float deltaTime) override;

    // ---- Output ----
    int  GetFrameIndex() const;
    const SpriteSheetAsset* GetSpriteSheet() const;

    bool IsPlaying() const { return m_Playing; }

    void SetSpeed(float speed) { m_Speed = speed; }

private:
    const AnimationClip*     m_CurrentClip  = nullptr;
    const SpriteSheetAsset*  m_SpriteSheet  = nullptr;

    bool  m_Playing   = false;
    bool  m_Loop      = true;

    float m_Speed     = 1.0f;
    float m_TimeInFrame = 0.0f;

    std::size_t m_ClipFrameIndex = 0;
};
