#include "assetmanager.h"

#include <fstream>
#include <sstream>
#include <iostream>

bool AssetManager::Initialize(const std::string& compiledAssetRoot)
{

    return true;
}

void AssetManager::Shutdown()
{
    for (auto& [id, tex] : m_textures)
    {
        delete[] tex.pixelData;
        tex.pixelData = nullptr;
    }

    m_textures.clear();
    m_spriteSheets.clear();
    m_animationSets.clear();
    m_objectAssets.clear();
}

bool AssetManager::LoadTexture(const AssetID& id,
                               const std::string& filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
        return false;

    // --- VERY SIMPLE TGA LOADER (uncompressed only) ---
    unsigned char header[18];
    file.read(reinterpret_cast<char*>(header), 18);

    int width  = header[12] | (header[13] << 8);
    int height = header[14] | (header[15] << 8);
    int bpp    = header[16];
    int channels = bpp / 8;

    int imageSize = width * height * channels;

    TextureAsset tex;
    tex.width = width;
    tex.height = height;
    tex.channels = channels;
    tex.pixelData = new unsigned char[imageSize];

    file.read(reinterpret_cast<char*>(tex.pixelData), imageSize);
    file.close();

    m_textures[id] = tex;
    return true;
}

bool AssetManager::LoadSpriteSheet(const AssetID& id,
                                   const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
        return false;

    SpriteSheetAsset sheet;
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        if (line.rfind("texture:", 0) == 0)
        {
            AssetID texId = line.substr(8);
            sheet.texture = &m_textures.at(texId);
        }
        else if (std::isdigit(line[0]))
        {
            SpriteFrame frame;
            sscanf(line.c_str(),
                   "%*d: x=%d y=%d w=%d h=%d",
                   &frame.x, &frame.y, &frame.w, &frame.h);

            sheet.frames.push_back(frame);
        }
    }

    m_spriteSheets[id] = sheet;
    return true;
}


bool AssetManager::LoadAnimationSet(const AssetID& id,
                                    const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
        return false;

    AnimationSetAsset animSet;
    AnimationClip* currentClip = nullptr;
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        if (line.rfind("clip:", 0) == 0)
        {
            AssetID name = line.substr(5);
            animSet.clips[name] = AnimationClip{};
            animSet.clips[name].name = name;
            currentClip = &animSet.clips[name];
        }
        else if (currentClip && std::isdigit(line[0]))
        {
            AnimationFrame frame;
            sscanf(line.c_str(),
                   "%*d: frame=%d duration=%f",
                   &frame.frameIndex, &frame.duration);

            currentClip->frames.push_back(frame);
        }
    }

    m_animationSets[id] = animSet;
    return true;
}

bool AssetManager::LoadObjectDescriptor(const ObjectID& objectId,
                                        const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
        return false;

    ObjectAssetDescriptor desc;
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        if (line.rfind("spritesheet:", 0) == 0)
            desc.spriteSheet = line.substr(12);
        else if (line.rfind("animations:", 0) == 0)
            desc.animationSet = line.substr(11);
    }

    m_objectAssets[objectId] = desc;
    return true;
}

const SpriteSheetAsset* AssetManager::GetSpriteSheet(const ObjectID& objectId) const
{
    const auto& obj = m_objectAssets.at(objectId);
    return &m_spriteSheets.at(obj.spriteSheet);
}

const AnimationClip* AssetManager::GetAnimation(const ObjectID& objectId,
                           const AssetID& animationName) const
{
    const auto& obj = m_objectAssets.at(objectId);
    const auto& animSet = m_animationSets.at(obj.animationSet);
    return &animSet.clips.at(animationName);
}

