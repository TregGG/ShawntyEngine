#pragma once

#include "component.h"
#include <cstdint>
#include <string>

// forward declarations (data-only)
struct AnimationSetAsset;
struct AnimationClip;
struct SpriteSheetAsset;

class AnimatorComponent
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
    bool IsActive() const { return m_Active; }

    void SetSpeed(float speed) { m_Speed = speed; }
    float GetSpeed() const { return m_Speed; }
    bool GetLoop() const { return m_Loop; }
    const std::string& GetClipName() const { return m_ClipName; }
    // Counts Play and Stop calls, to notice when a clip is restarted
    uint8_t GetCommandCount() const { return m_Commands; }
    // Stopped by Stop(), as opposed to a clip that reached its end
    bool IsStopped() const { return m_Stopped; }

    std::string objectId; // which object asset the animations came from

    // ---------- Output (used by Scene) ----------
    int GetFrameIndex() const;
    const SpriteSheetAsset* GetSpriteSheet() const;

    // ---------- Component ----------
    void Update(float deltaTime);

private:
    // Bound assets (non-owning)
    const AnimationSetAsset* m_AnimSet     = nullptr;
    const SpriteSheetAsset*  m_SpriteSheet = nullptr;

    // Runtime state
    const AnimationClip* m_CurrentClip = nullptr;

    std::string m_ClipName;
    uint8_t m_Commands = 0;
    bool   m_Stopped = false;
    bool   m_Playing = false;
    bool   m_Loop    = true;

    float  m_Speed        = 1.0f;
    float  m_TimeInFrame  = 0.0f;
    size_t m_ClipFrameIdx = 0;
    bool   m_Active       = true;
};
