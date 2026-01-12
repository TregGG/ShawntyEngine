#include "assetmanager.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

#include <glad/glad.h>
#include <filesystem>

namespace fs = std::filesystem;

// ------------------------------------------------------------
// Helper: very simple uncompressed TGA loader (local CPU buffer)
// ------------------------------------------------------------
static bool LoadTGA(const std::string& filepath,
                    int& outWidth,
                    int& outHeight,
                    int& outChannels,
                    std::vector<unsigned char>& outPixels)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
        return false;

    unsigned char header[18];
    file.read(reinterpret_cast<char*>(header), 18);

    outWidth  = header[12] | (header[13] << 8);
    outHeight = header[14] | (header[15] << 8);
    int bpp   = header[16];

    outChannels = bpp / 8;
    if (outChannels != 3 && outChannels != 4)
        return false;

    int imageSize = outWidth * outHeight * outChannels;
    outPixels.resize(imageSize);

    file.read(reinterpret_cast<char*>(outPixels.data()), imageSize);
    file.close();

    return true;
}

// ------------------------------------------------------------
// AssetManager lifecycle
// ------------------------------------------------------------
bool AssetManager::Initialize(const std::string& compiledAssetRoot)
{
    fs::path root(compiledAssetRoot);

    // ---------- 1. Load Textures (.tga → GPU) ----------
    fs::path textureDir = root / "textures";
    for (const auto& entry : fs::directory_iterator(textureDir))
    {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() == ".tga")
        {
            AssetID id = entry.path().stem().string();
            LoadTexture(id, entry.path().string());
        }
    }

    // ---------- 2. Load Sprite Sheets (.ssheet) ----------
    fs::path sheetDir = root / "spritesheets";
    for (const auto& entry : fs::directory_iterator(sheetDir))
    {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() == ".ssheet")
        {
            AssetID id = entry.path().stem().string();
            LoadSpriteSheet(id, entry.path().string());
        }
    }

    // ---------- 3. Load Animation Sets (.anim) ----------
    fs::path animDir = root / "animations";
    for (const auto& entry : fs::directory_iterator(animDir))
    {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() == ".anim")
        {
            AssetID id = entry.path().stem().string();
            LoadAnimationSet(id, entry.path().string());
        }
    }

    // ---------- 4. Load Object Descriptors (.objasset) ----------
    fs::path objectDir = root / "objects";
    for (const auto& entry : fs::directory_iterator(objectDir))
    {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() == ".objasset")
        {
            ObjectID objectId = entry.path().stem().string();
            LoadObjectDescriptor(objectId, entry.path().string());
        }
    }

    return true;
}

void AssetManager::Shutdown()
{
    // Free GPU textures
    for (auto& [id, tex] : m_textures)
    {
        if (tex.handle != 0)
            glDeleteTextures(1, &tex.handle);
    }

    m_textures.clear();
    m_spriteSheets.clear();
    m_animationSets.clear();
    m_objectAssets.clear();
}

// ------------------------------------------------------------
// Texture loading (CPU → GPU → discard CPU)
// ------------------------------------------------------------
bool AssetManager::LoadTexture(const AssetID& id,
                               const std::string& filepath)
{
    int width = 0, height = 0, channels = 0;
    std::vector<unsigned char> pixels;

    if (!LoadTGA(filepath, width, height, channels, pixels))
    {
        std::cerr << "Failed to load TGA: " << filepath << "\n";
        return false;
    }

    TextureGPU gpuTex;
    gpuTex.width  = width;
    gpuTex.height = height;

    glGenTextures(1, &gpuTex.handle);
    glBindTexture(GL_TEXTURE_2D, gpuTex.handle);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 format,
                 width,
                 height,
                 0,
                 format,
                 GL_UNSIGNED_BYTE,
                 pixels.data());

    glBindTexture(GL_TEXTURE_2D, 0);

    m_textures[id] = gpuTex;
    return true;
}

// ------------------------------------------------------------
// Sprite sheet loading
// ------------------------------------------------------------
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
            sheet.texture = &m_textures.at(texId); // TextureGPU*
        }
        else if (std::isdigit(line[0]))
        {
            SpriteFrame frame{};
            sscanf(line.c_str(),
                   "%*d: x=%d y=%d w=%d h=%d",
                   &frame.x, &frame.y, &frame.w, &frame.h);

            sheet.frames.push_back(frame);
        }
    }

    m_spriteSheets[id] = sheet;
    return true;
}

// ------------------------------------------------------------
// Animation loading
// ------------------------------------------------------------
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
            AnimationFrame frame{};
            sscanf(line.c_str(),
                   "%*d: frame=%d duration=%f",
                   &frame.frameIndex, &frame.duration);

            currentClip->frames.push_back(frame);
        }
    }

    m_animationSets[id] = animSet;
    return true;
}

// ------------------------------------------------------------
// Object descriptor loading
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// Asset accessors
// ------------------------------------------------------------
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
