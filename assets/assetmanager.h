#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "assetdatastruct.h"



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
    std::unordered_map<AssetID, TextureGPU>      m_textures;
    std::unordered_map<AssetID, SpriteSheetAsset>  m_spriteSheets;
    std::unordered_map<AssetID, AnimationSetAsset> m_animationSets;

    // object → asset lookup
    std::unordered_map<ObjectID, ObjectAssetDescriptor> m_objectAssets;
};
