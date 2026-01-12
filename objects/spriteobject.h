#pragma once
#include "gameobject.h"
#include"../assets/assetdatastruct.h"
#include "components/animator.h"

struct SpriteSheetAsset;
struct AnimationClip;

class SpriteObject : public GameObject
{
public:
    SpriteObject() = default;
    explicit SpriteObject(const std::string& name)
        : GameObject(name) {}

    // ---- Animator access ----
    Animator& GetAnimator() { return m_Animator; }
    const Animator& GetAnimator() const { return m_Animator; }

    // ---- Convenience helpers (optional but useful) ----
    void PlayAnimation(const AnimationClip* clip,
                       const SpriteSheetAsset* sheet,
                       bool loop = true)
    {
        m_Animator.Play(clip, sheet, loop);
    }

    // ---- Sprite info (read-only for renderer/scene) ----
    const SpriteSheetAsset* GetSpriteSheet() const
    {
        return m_Animator.GetSpriteSheet();
    }

    int GetFrameIndex() const
    {
        return m_Animator.GetFrameIndex();
    }

    // ---- Update hook ----
    void Update(float deltaTime) override
    {
        m_Animator.Update(deltaTime);
    }

private:
    Animator m_Animator;
};

