#pragma once

#include <string>
#include <vector>
#include <unordered_map>

using AssetID  = std::string;
using ObjectID = std::string;

// --------------------
// GPU TEXTURE (renderer-facing)
// --------------------
struct TextureGPU
{
    unsigned int handle = 0; // OpenGL texture ID
    int width  = 0;
    int height = 0;
};

// --------------------
// SPRITE SHEET (.ssheet)
// --------------------
struct SpriteFrame
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct SpriteSheetAsset
{
    const TextureGPU* texture = nullptr; // non-owning, GPU-ready
    std::vector<SpriteFrame> frames;
};

// --------------------
// ANIMATION (.anim)
// --------------------
struct AnimationFrame
{
    int frameIndex = 0;   // index into SpriteSheetAsset::frames
    float duration = 0.f;
};

struct AnimationClip
{
    AssetID name;
    std::vector<AnimationFrame> frames;
};

struct AnimationSetAsset
{
    std::unordered_map<AssetID, AnimationClip> clips;
};

// --------------------
// OBJECT → ASSET MAPPING
// --------------------
struct ObjectAssetDescriptor
{
    AssetID spriteSheet;    // reference to SpriteSheetAsset
    AssetID animationSet;   // reference to AnimationSetAsset
};
