#pragma once

#include <string>
#include <unordered_map>
#include <glm/vec2.hpp>
#include "../core/entityid.h"

class Scene;
class AssetManager;
class FontEngine;
class EventService;

class SceneSerializer {
public:
    struct SceneLoadResult {
        bool success = false;
        std::unordered_map<std::string, EntityID> editorIdMap; // editorId -> runtime EntityID
        std::string errorMessage;
    };

    // Loads a .scene JSON file and populates the given Scene's registry
    static SceneLoadResult LoadScene(const std::string& filepath,
                                     Scene* scene,
                                     AssetManager* assets,
                                     FontEngine* fontEngine = nullptr,
                                     EventService* eventService = nullptr);

    // Saves current scene state to a .scene JSON file
    static bool SaveScene(const std::string& filepath,
                          const Scene* scene);

    // Loads a .prefab JSON and instantiates it into the scene registry
    // Returns the root EntityID of the instantiated prefab
    static EntityID InstantiatePrefab(const std::string& filepath,
                                      Scene* scene,
                                      AssetManager* assets,
                                      const glm::vec2& position = {0.0f, 0.0f},
                                      std::unordered_map<std::string, EntityID>* outEditorIdMap = nullptr);
};
