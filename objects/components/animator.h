#pragma once

#include "component.h"
#include <string>

// forward declarations (data-only)
struct AnimationSetAsset;
struct AnimationClip;
struct SpriteSheetAsset;

class AnimatorComponent : public Component
{
public:
    AnimatorComponent() = default;
    virtual ~AnimatorComponent() = default;

    // ---------- Binding (one-time setup) ----------
    void BindAnimationSet(const AnimationSetAsset* animSet,
                          const SpriteSheetAsset* spriteSheet);

    // ---------- Playback ----------
    void Play(const std::string& clipName, bool loop = true);
    void Stop();

    bool HasAnimation(const std::string& clipName) const;
    bool IsPlaying() const { return m_Playing; }

    void SetSpeed(float speed) { m_Speed = speed; }

    // ---------- Output (used by Scene) ----------
    int GetFrameIndex() const;
    const SpriteSheetAsset* GetSpriteSheet() const;

    // ---------- Component ----------
    void Update(float deltaTime) override;

private:
    // Bound assets (non-owning)
    const AnimationSetAsset* m_AnimSet     = nullptr;
    const SpriteSheetAsset*  m_SpriteSheet = nullptr;

    // Runtime state
    const AnimationClip* m_CurrentClip = nullptr;

    bool   m_Playing = false;
    bool   m_Loop    = true;

    float  m_Speed        = 1.0f;
    float  m_TimeInFrame  = 0.0f;
    size_t m_ClipFrameIdx = 0;
};
