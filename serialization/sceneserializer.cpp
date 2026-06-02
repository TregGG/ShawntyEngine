#include "sceneserializer.h"

#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

#include "../levels/scene.h"
#include "../assets/assetmanager.h"
#include "../objects/components/components.h"
#include "../objects/components/collidercomponent.h"
#include "../objects/components/rigidbodycomponent.h"
#include "../objects/components/animator.h"
#include "../objects/components/scriptcomponent.h"
#include "../objects/ui/uipanel.h"
#include "../objects/ui/uitext.h"
#include "../objects/ui/uibutton.h"
#include "../objects/ui/uiinputfield.h"

#define ENGINE_CLASS "SceneSerializer"
#include "../core/enginedebug.h"

using json = nlohmann::json;

// ============================================================
// Helper functions
// ============================================================
namespace {

EntityCategory ParseCategory(const std::string& str) {
    if (str == "Environment") return EntityCategory::Environment;
    if (str == "Enemy")       return EntityCategory::Enemy;
    if (str == "Projectile")  return EntityCategory::Projectile;
    if (str == "Player")      return EntityCategory::Player;
    if (str == "UI")          return EntityCategory::UI;
    return EntityCategory::Environment;
}

Layer ParseLayer(const std::string& str) {
    if (str == "UI")         return Layer::UI;
    if (str == "Player")     return Layer::Player;
    if (str == "Foreground") return Layer::Foreground;
    if (str == "Background") return Layer::Background;
    return Layer::Foreground;
}

BodyType ParseBodyType(const std::string& str) {
    if (str == "Static")    return BodyType::Static;
    if (str == "Kinematic") return BodyType::Kinematic;
    if (str == "Dynamic")   return BodyType::Dynamic;
    return BodyType::Static;
}

std::string CategoryToString(EntityCategory cat) {
    switch (cat) {
        case EntityCategory::Environment: return "Environment";
        case EntityCategory::Enemy:       return "Enemy";
        case EntityCategory::Projectile:  return "Projectile";
        case EntityCategory::Player:      return "Player";
        case EntityCategory::UI:          return "UI";
        default: return "Environment";
    }
}

std::string LayerToString(Layer layer) {
    switch (layer) {
        case Layer::UI:         return "UI";
        case Layer::Player:     return "Player";
        case Layer::Foreground: return "Foreground";
        case Layer::Background: return "Background";
        default: return "Foreground";
    }
}

std::string BodyTypeToString(BodyType type) {
    switch (type) {
        case BodyType::Static:    return "Static";
        case BodyType::Kinematic: return "Kinematic";
        case BodyType::Dynamic:   return "Dynamic";
        default: return "Static";
    }
}

// Read a vec2 from a JSON array [x, y]
glm::vec2 ReadVec2(const json& j, const std::string& key, const glm::vec2& defaultVal = {0.0f, 0.0f}) {
    if (j.contains(key) && j[key].is_array() && j[key].size() >= 2) {
        return glm::vec2(j[key][0].get<float>(), j[key][1].get<float>());
    }
    return defaultVal;
}

// Load components from a JSON "components" object onto an entity
void LoadComponents(const json& compJson, EntityID entityId,
                    EntityRegistryService& registry, AssetManager* assets) {

    // --- Transform ---
    if (compJson.contains("transform")) {
        const auto& t = compJson["transform"];
        TransformComponent tc;
        tc.position = ReadVec2(t, "position");
        tc.localPosition = ReadVec2(t, "localPosition");
        tc.size = ReadVec2(t, "size", {1.0f, 1.0f});
        tc.rotation = t.value("rotation", 0.0f);
        registry.AddComponent<TransformComponent>(entityId, tc);
    }

    // --- Sprite ---
    if (compJson.contains("sprite")) {
        const auto& s = compJson["sprite"];
        SpriteComponent2D sc;
        sc.frameIndex = s.value("frameIndex", 0);
        sc.layer = ParseLayer(s.value("layer", "Foreground"));

        std::string objectId = s.value("objectId", "");
        if (!objectId.empty() && assets) {
            try {
                sc.spriteSheet = assets->GetSpriteSheet(objectId);
            } catch (const std::exception& e) {
                ENGINE_WARN("Failed to resolve spriteSheet for objectId '%s': %s",
                            objectId.c_str(), e.what());
                sc.spriteSheet = nullptr;
            }
        } else {
            sc.spriteSheet = nullptr;
        }

        registry.AddComponent<SpriteComponent2D>(entityId, sc);
    }

    // --- Collider ---
    if (compJson.contains("collider")) {
        const auto& c = compJson["collider"];
        glm::vec2 localOffset = ReadVec2(c, "localOffset");
        glm::vec2 localSize = ReadVec2(c, "localSize", {1.0f, 1.0f});
        bool isTrigger = c.value("isTrigger", false);

        ColliderComponent cc(localOffset, localSize, isTrigger);
        cc.SetAutoBounds(c.value("autoBounds", false));
        cc.SetLayerMask(c.value("layerMask", (uint32_t)0xFFFFFFFF));

        registry.AddComponent<ColliderComponent>(entityId, cc);
    }

    // --- RigidBody ---
    if (compJson.contains("rigidbody")) {
        const auto& r = compJson["rigidbody"];
        RigidBodyComponent rb;
        rb.SetType(ParseBodyType(r.value("bodyType", "Static")));
        rb.SetMass(r.value("mass", 1.0f));
        rb.SetDrag(r.value("drag", 0.0f));
        rb.SetGravityScale(r.value("gravityScale", 1.0f));
        rb.SetUseGravity(r.value("useGravity", false));
        rb.SetElasticity(r.value("elasticity", 0.0f));

        registry.AddComponent<RigidBodyComponent>(entityId, rb);
    }

    // --- Animator ---
    if (compJson.contains("animator")) {
        const auto& a = compJson["animator"];
        AnimatorComponent ac;

        std::string objectId = a.value("objectId", "");
        if (!objectId.empty() && assets) {
            try {
                const SpriteSheetAsset* sheet = assets->GetSpriteSheet(objectId);
                const AnimationSetAsset* animSet = assets->GetAnimationSet(objectId);
                ac.BindAnimationSet(animSet, sheet);
            } catch (const std::exception& e) {
                ENGINE_WARN("Failed to resolve animator assets for objectId '%s': %s",
                            objectId.c_str(), e.what());
            }
        }

        ac.SetSpeed(a.value("speed", 1.0f));

        std::string defaultClip = a.value("defaultClip", "");
        bool loop = a.value("loop", true);
        if (!defaultClip.empty()) {
            ac.Play(defaultClip, loop);
        }

        registry.AddComponent<AnimatorComponent>(entityId, ac);
    }

    // --- Script (data-only, Phase 2 will execute) ---
    if (compJson.contains("script")) {
        const auto& sc = compJson["script"];
        ScriptComponent scriptComp;
        scriptComp.scriptPath = sc.value("path", "");
        scriptComp.className = sc.value("class", "");

        if (sc.contains("properties") && sc["properties"].is_object()) {
            for (auto& [key, val] : sc["properties"].items()) {
                scriptComp.properties[key] = val.dump();
            }
        }

        registry.AddComponent<ScriptComponent>(entityId, scriptComp);
    }
}

// Load a single entity from JSON, return its EntityID
EntityID LoadEntity(const json& entityJson, Scene* scene, AssetManager* assets,
                    std::unordered_map<std::string, EntityID>& editorIdMap) {

    std::string name = entityJson.value("name", "Entity");
    EntityCategory category = ParseCategory(entityJson.value("category", "Environment"));

    EntityID entityId = scene->CreateEntity(category, name);

    // Store editor ID mapping
    if (entityJson.contains("editorId")) {
        std::string editorId = entityJson["editorId"].get<std::string>();
        editorIdMap[editorId] = entityId;
        scene->registry.SetEditorId(entityId, editorId);
    }

    // Load components
    if (entityJson.contains("components")) {
        LoadComponents(entityJson["components"], entityId, scene->registry, assets);
    }

    return entityId;
}

std::unique_ptr<UIObject> LoadUIElement(const json& uiJson, Scene* scene, FontEngine* fontEngine, EventService* eventService) {
    std::string type = uiJson.value("type", "Panel");
    std::string name = uiJson.value("name", "UIElement");
    
    std::unique_ptr<UIObject> el;
    
    if (type == "Panel") {
        el = std::make_unique<UIPanel>(scene, name);
    } else if (type == "Text") {
        auto t = std::make_unique<UIText>(scene, name, fontEngine);
        t->Text = uiJson.value("text", "Text");
        t->TextColor = glm::vec3(uiJson.value("textColor", std::vector<float>{1.0f, 1.0f, 1.0f}).data());
        el = std::move(t);
    } else if (type == "Button") {
        auto b = std::make_unique<UIButton>(scene, name, eventService);
        b->ActionType = uiJson.value("action", "");
        b->ActionTarget = uiJson.value("actionTarget", "");
        if (uiJson.contains("text")) {
            auto t = std::make_unique<UIText>(scene, name + "_text", fontEngine);
            t->Text = uiJson.value("text", "Button");
            b->AddChild(std::move(t));
        }
        el = std::move(b);
    } else if (type == "Input") {
        auto i = std::make_unique<UIInputField>(scene, name, eventService, fontEngine);
        if (i->GetTextElement()) {
            i->GetTextElement()->Text = uiJson.value("text", "");
        }
        el = std::move(i);
    } else {
        el = std::make_unique<UIPanel>(scene, name);
    }

    el->Position = ReadVec2(uiJson, "position");
    el->Size = ReadVec2(uiJson, "size", {100.0f, 40.0f});
    
    if (uiJson.contains("backgroundColor") && uiJson["backgroundColor"].is_array() && uiJson["backgroundColor"].size() >= 4) {
        el->BackgroundColor = glm::vec4(
            uiJson["backgroundColor"][0].get<float>(),
            uiJson["backgroundColor"][1].get<float>(),
            uiJson["backgroundColor"][2].get<float>(),
            uiJson["backgroundColor"][3].get<float>()
        );
    }

    if (uiJson.contains("children") && uiJson["children"].is_array()) {
        for (const auto& childJson : uiJson["children"]) {
            if (auto childEl = LoadUIElement(childJson, scene, fontEngine, eventService)) {
                el->AddChild(std::move(childEl));
            }
        }
    }

    return el;
}

} // anonymous namespace

// ============================================================
// LoadScene
// ============================================================
SceneSerializer::SceneLoadResult SceneSerializer::LoadScene(
    const std::string& filepath, Scene* scene, AssetManager* assets,
    FontEngine* fontEngine, EventService* eventService) {

    SceneLoadResult result;

    // Read file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        result.errorMessage = "Failed to open scene file: " + filepath;
        ENGINE_ERROR("%s", result.errorMessage.c_str());
        return result;
    }

    // Parse JSON
    json sceneJson;
    try {
        sceneJson = json::parse(file);
    } catch (const json::parse_error& e) {
        result.errorMessage = "JSON parse error in " + filepath + ": " + e.what();
        ENGINE_ERROR("%s", result.errorMessage.c_str());
        return result;
    }

    if (!sceneJson.contains("scene")) {
        result.errorMessage = "Missing 'scene' root object in " + filepath;
        ENGINE_ERROR("%s", result.errorMessage.c_str());
        return result;
    }

    const auto& sceneData = sceneJson["scene"];

    // Camera settings
    if (sceneData.contains("camera")) {
        const auto& cam = sceneData["camera"];
        glm::vec2 camPos = ReadVec2(cam, "position");
        float camScale = cam.value("scale", 1.0f);
        scene->GetCamera().SetCameraPosition(camPos.x, camPos.y);
        scene->GetCamera().SetScale(camScale);
    }

    // Load entities
    if (sceneData.contains("entities") && sceneData["entities"].is_array()) {
        for (const auto& entityJson : sceneData["entities"]) {
            LoadEntity(entityJson, scene, assets, result.editorIdMap);
        }
    }

    // Wire relationships
    if (sceneData.contains("relationships") && sceneData["relationships"].is_array()) {
        for (const auto& rel : sceneData["relationships"]) {
            std::string parentEditorId = rel.value("parent", "");
            auto parentIt = result.editorIdMap.find(parentEditorId);
            if (parentIt == result.editorIdMap.end()) {
                ENGINE_WARN("Relationship references unknown parent editorId: '%s'", parentEditorId.c_str());
                continue;
            }
            EntityID parentId = parentIt->second;

            RelationshipComponent parentRel;

            if (rel.contains("children") && rel["children"].is_array()) {
                for (const auto& childIdJson : rel["children"]) {
                    std::string childEditorId = childIdJson.get<std::string>();
                    auto childIt = result.editorIdMap.find(childEditorId);
                    if (childIt == result.editorIdMap.end()) {
                        ENGINE_WARN("Relationship references unknown child editorId: '%s'", childEditorId.c_str());
                        continue;
                    }
                    EntityID childId = childIt->second;
                    parentRel.children.push_back(childId);

                    // Child relationship
                    RelationshipComponent childRel;
                    childRel.parent = parentId;
                    scene->registry.AddComponent<RelationshipComponent>(childId, childRel);

                    // Wire parent transform pointer
                    if (scene->registry.HasComponent<TransformComponent>(childId) &&
                        scene->registry.HasComponent<TransformComponent>(parentId)) {
                        scene->registry.GetComponent<TransformComponent>(childId).parentTransform =
                            &scene->registry.GetComponent<TransformComponent>(parentId);
                    }
                }
            }

            scene->registry.AddComponent<RelationshipComponent>(parentId, parentRel);
        }
    }

    // Load UI
    if (sceneData.contains("ui") && sceneData["ui"].is_array()) {
        for (const auto& uiJson : sceneData["ui"]) {
            if (auto el = LoadUIElement(uiJson, scene, fontEngine, eventService)) {
                scene->registry.AddUIElement(std::move(el));
            }
        }
    }

    int entityCount = 0;
    if (sceneData.contains("entities") && sceneData["entities"].is_array()) {
        entityCount = static_cast<int>(sceneData["entities"].size());
    }
    ENGINE_LOG("Scene loaded from '%s': %d entities", filepath.c_str(), entityCount);

    result.success = true;
    return result;
}

// ============================================================
// SaveScene
// ============================================================
bool SceneSerializer::SaveScene(const std::string& filepath, const Scene* scene) {
    json sceneJson;
    sceneJson["version"] = 1;

    json sceneData;
    sceneData["name"] = "SavedScene";

    // Camera
    json cam;
    auto camPos = scene->GetCamera().GetCameraPosition();
    cam["position"] = {camPos.x, camPos.y};
    cam["scale"] = scene->GetCamera().GetScale();
    sceneData["camera"] = cam;

    // Entities
    json entitiesArray = json::array();

    // Iterate through all category buckets to find alive entities
    for (int c = 0; c < static_cast<int>(EntityCategory::Count); ++c) {
        EntityCategory cat = static_cast<EntityCategory>(c);
        const auto& bucket = scene->registry.GetEntities(cat);

        for (std::uint32_t index : bucket) {
            // Since we're iterating buckets, the entity should be alive
            // We need the real EntityID — reconstruct using slot data
            // For now, just use index with generation 0 for the lookup
            // The actual EntityID doesn't matter for serialization, we just need data

            // Build a temporary EntityID — we iterate alive entities from buckets
            // so we know the index is valid. Generation doesn't matter for Get/Has.
            EntityID eid = MakeEntityID(index, 0);

            json entityJson;
            entityJson["name"] = std::string(scene->registry.GetName(eid));
            entityJson["category"] = CategoryToString(cat);

            const auto& editorId = scene->registry.GetEditorId(eid);
            if (!editorId.empty()) {
                entityJson["editorId"] = editorId;
            }

            json comps;

            // Transform
            if (scene->registry.HasComponent<TransformComponent>(eid)) {
                const auto& t = scene->registry.GetComponent<TransformComponent>(eid);
                json tj;
                tj["position"] = {t.position.x, t.position.y};
                tj["localPosition"] = {t.localPosition.x, t.localPosition.y};
                tj["size"] = {t.size.x, t.size.y};
                tj["rotation"] = t.rotation;
                comps["transform"] = tj;
            }

            // Sprite
            if (scene->registry.HasComponent<SpriteComponent2D>(eid)) {
                const auto& s = scene->registry.GetComponent<SpriteComponent2D>(eid);
                json sj;
                sj["frameIndex"] = s.frameIndex;
                sj["layer"] = LayerToString(s.layer);
                // Note: we can't reverse-resolve objectId from spriteSheet pointer
                // The editor will need to maintain this mapping
                comps["sprite"] = sj;
            }

            // Collider
            if (scene->registry.HasComponent<ColliderComponent>(eid)) {
                const auto& c = scene->registry.GetComponent<ColliderComponent>(eid);
                json cj;
                cj["isTrigger"] = c.IsTrigger();
                cj["autoBounds"] = c.GetAutoBounds();
                cj["layerMask"] = c.GetLayerMask();
                comps["collider"] = cj;
            }

            // RigidBody
            if (scene->registry.HasComponent<RigidBodyComponent>(eid)) {
                const auto& r = scene->registry.GetComponent<RigidBodyComponent>(eid);
                json rj;
                rj["bodyType"] = BodyTypeToString(r.GetType());
                rj["mass"] = r.GetMass();
                rj["drag"] = r.GetDrag();
                rj["gravityScale"] = r.GetGravityScale();
                rj["useGravity"] = r.GetUseGravity();
                rj["elasticity"] = r.GetElasticity();
                comps["rigidbody"] = rj;
            }

            // Animator
            if (scene->registry.HasComponent<AnimatorComponent>(eid)) {
                json aj;
                aj["speed"] = 1.0f; // Can't read speed back from AnimatorComponent currently
                comps["animator"] = aj;
            }

            // Script
            if (scene->registry.HasComponent<ScriptComponent>(eid)) {
                const auto& sc = scene->registry.GetComponent<ScriptComponent>(eid);
                json scj;
                scj["path"] = sc.scriptPath;
                scj["class"] = sc.className;
                json props;
                for (const auto& [k, v] : sc.properties) {
                    props[k] = v;
                }
                scj["properties"] = props;
                comps["script"] = scj;
            }

            entityJson["components"] = comps;
            entitiesArray.push_back(entityJson);
        }
    }

    sceneData["entities"] = entitiesArray;

    // Relationships
    json relsArray = json::array();
    for (EntityID eid : scene->registry.ViewRelationships()) {
        const auto& rel = scene->registry.GetComponent<RelationshipComponent>(eid);
        if (rel.children.empty()) continue; // Only save parent entries

        const auto& parentEditorId = scene->registry.GetEditorId(eid);
        if (parentEditorId.empty()) continue;

        json relJson;
        relJson["parent"] = parentEditorId;
        json childrenArr = json::array();
        for (EntityID child : rel.children) {
            const auto& childEditorId = scene->registry.GetEditorId(child);
            if (!childEditorId.empty()) {
                childrenArr.push_back(childEditorId);
            }
        }
        relJson["children"] = childrenArr;
        relsArray.push_back(relJson);
    }
    sceneData["relationships"] = relsArray;

    // Serialize UI
    auto saveUIElement = [](const UIObject* el, auto& saveUIElementRef) -> json {
        json uiJson;
        uiJson["name"] = el->GetName();
        uiJson["position"] = {el->Position.x, el->Position.y};
        uiJson["size"] = {el->Size.x, el->Size.y};
        uiJson["backgroundColor"] = {el->BackgroundColor.r, el->BackgroundColor.g, el->BackgroundColor.b, el->BackgroundColor.a};

        if (dynamic_cast<const UIPanel*>(el)) uiJson["type"] = "Panel";
        if (auto t = dynamic_cast<const UIText*>(el)) {
            uiJson["type"] = "Text";
            uiJson["text"] = t->Text;
            uiJson["textColor"] = {t->TextColor.r, t->TextColor.g, t->TextColor.b};
        } else if (auto b = dynamic_cast<const UIButton*>(el)) {
            uiJson["type"] = "Button";
            uiJson["action"] = b->ActionType;
            uiJson["actionTarget"] = b->ActionTarget;
            // The text is a child but for simplicity in editor, we handle it
        } else if (auto i = dynamic_cast<const UIInputField*>(el)) {
            uiJson["type"] = "Input";
            if (i->GetTextElement()) uiJson["text"] = i->GetTextElement()->Text;
        }

        json childrenArr = json::array();
        for (const auto& child : el->GetChildren()) {
            childrenArr.push_back(saveUIElementRef(child.get(), saveUIElementRef));
        }
        if (!childrenArr.empty()) uiJson["children"] = childrenArr;

        return uiJson;
    };

    json uiArray = json::array();
    for (const auto& el : scene->registry.GetUIElements()) {
        uiArray.push_back(saveUIElement(el.get(), saveUIElement));
    }
    sceneData["ui"] = uiArray;

    sceneJson["scene"] = sceneData;

    // Write to file
    std::ofstream out(filepath);
    if (!out.is_open()) {
        ENGINE_ERROR("Failed to open file for saving: %s", filepath.c_str());
        return false;
    }

    out << sceneJson.dump(4);
    out.close();

    ENGINE_LOG("Scene saved to '%s'", filepath.c_str());
    return true;
}

// ============================================================
// InstantiatePrefab
// ============================================================
EntityID SceneSerializer::InstantiatePrefab(
    const std::string& filepath, Scene* scene, AssetManager* assets,
    const glm::vec2& position, std::unordered_map<std::string, EntityID>* outEditorIdMap) {

    // Read file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        ENGINE_ERROR("Failed to open prefab file: %s", filepath.c_str());
        return 0;
    }

    json prefabJson;
    try {
        prefabJson = json::parse(file);
    } catch (const json::parse_error& e) {
        ENGINE_ERROR("JSON parse error in prefab %s: %s", filepath.c_str(), e.what());
        return 0;
    }

    if (!prefabJson.contains("prefab")) {
        ENGINE_ERROR("Missing 'prefab' root object in %s", filepath.c_str());
        return 0;
    }

    const auto& prefabData = prefabJson["prefab"];
    std::unordered_map<std::string, EntityID> localEditorIdMap;

    // Create root entity
    std::string name = prefabData.value("name", "PrefabEntity");
    EntityCategory category = ParseCategory(prefabData.value("category", "Environment"));

    EntityID rootId = scene->CreateEntity(category, name);

    // Load components
    if (prefabData.contains("components")) {
        LoadComponents(prefabData["components"], rootId, scene->registry, assets);
    }

    // Override position if transform was loaded
    if (scene->registry.HasComponent<TransformComponent>(rootId)) {
        scene->registry.GetComponent<TransformComponent>(rootId).position = position;
    }

    // Store editor ID for root
    if (prefabData.contains("editorId")) {
        std::string editorId = prefabData["editorId"].get<std::string>();
        localEditorIdMap[editorId] = rootId;
        scene->registry.SetEditorId(rootId, editorId);
    }

    // Process children
    if (prefabData.contains("children") && prefabData["children"].is_array()) {
        RelationshipComponent parentRel;

        for (const auto& childJson : prefabData["children"]) {
            std::string childName = childJson.value("name", "Child");
            EntityCategory childCat = ParseCategory(childJson.value("category", "Environment"));

            EntityID childId = scene->CreateEntity(childCat, childName);

            // Load child components
            if (childJson.contains("components")) {
                LoadComponents(childJson["components"], childId, scene->registry, assets);
            }

            // Child editor ID
            if (childJson.contains("editorId")) {
                std::string childEditorId = childJson["editorId"].get<std::string>();
                localEditorIdMap[childEditorId] = childId;
                scene->registry.SetEditorId(childId, childEditorId);
            }

            // Parent-child relationship
            parentRel.children.push_back(childId);

            RelationshipComponent childRel;
            childRel.parent = rootId;
            scene->registry.AddComponent<RelationshipComponent>(childId, childRel);

            // Wire transform parent pointer
            if (scene->registry.HasComponent<TransformComponent>(childId) &&
                scene->registry.HasComponent<TransformComponent>(rootId)) {
                scene->registry.GetComponent<TransformComponent>(childId).parentTransform =
                    &scene->registry.GetComponent<TransformComponent>(rootId);
            }
        }

        scene->registry.AddComponent<RelationshipComponent>(rootId, parentRel);
    }

    // Copy editor ID map to output if requested
    if (outEditorIdMap) {
        for (const auto& [k, v] : localEditorIdMap) {
            (*outEditorIdMap)[k] = v;
        }
    }

    ENGINE_LOG("Prefab '%s' instantiated from '%s'", name.c_str(), filepath.c_str());
    return rootId;
}
