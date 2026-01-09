#pragma once

#include <string>
#include <vector>
#include <unordered_map>

using AssetID  = std::string;   // "player_sheet", "player_anims"
using ObjectID = std::string;   // "Player"


// TEXTURE (.tga)
struct TextureAsset
{
    int width  = 0;
    int height = 0;
    int channels = 0; // usually 4 (RGBA)

    unsigned char* pixelData = nullptr; // owned by AssetManager
};


// SPRITE SHEET (.ssheet)
struct SpriteFrame
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct SpriteSheetAsset
{
    const TextureAsset* texture = nullptr; // non-owning
    std::vector<SpriteFrame> frames;
};


// ANIMATION (.anim)
struct AnimationFrame
{
    int frameIndex = 0;   // index into SpriteSheetAsset::frames
    float duration = 0.f; // seconds
};

struct AnimationClip
{
    AssetID name; // "Player_run", "Player_idle"
    std::vector<AnimationFrame> frames;
};

struct AnimationSetAsset
{
    std::unordered_map<AssetID, AnimationClip> clips;
};


// OBJECT → ASSET MAPPING
struct ObjectAssetDescriptor
{
    AssetID spriteSheet;   // which .ssheet + .tga
    AssetID animationSet;  // which .anim
};


// ASSET MANAGER (DATA-ONLY)
class AssetManager
{
public:
    bool Initialize(const std::string& compiledAssetRoot);
    void Shutdown();

    // object-level queries
    const SpriteSheetAsset* GetSpriteSheet(const ObjectID& objectId) const;
    const AnimationClip* GetAnimation(const ObjectID& objectId,
                                      const AssetID& animationName) const;

protected:
    // internal loading helpers (NOT part of public API)
    bool LoadTexture(const AssetID& id, const std::string& filepath);
    bool LoadSpriteSheet(const AssetID& id, const std::string& filepath);
    bool LoadAnimationSet(const AssetID& id, const std::string& filepath);
    bool LoadObjectDescriptor(const ObjectID& objectId,
                              const std::string& filepath);

private:
    // raw loaded assets
    std::unordered_map<AssetID, TextureAsset>      m_textures;
    std::unordered_map<AssetID, SpriteSheetAsset>  m_spriteSheets;
    std::unordered_map<AssetID, AnimationSetAsset> m_animationSets;

    // object → asset lookup
    std::unordered_map<ObjectID, ObjectAssetDescriptor> m_objectAssets;
};
