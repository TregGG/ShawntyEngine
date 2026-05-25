#pragma once
#include "../levels/scene.h"
#include <vector>
#include <memory>
#include <glm/vec2.hpp>
#include "../services/base/entityregistry/entityregistry.h"
#include "../services/base/physics/physicssystem.h"
#include "../services/base/raycast.h"

class GameObject;
class AssetManager;
class Input;

#include "../objects/ui/uipanel.h"
#include "../objects/ui/uitext.h"
#include "../objects/ui/uibutton.h"
#include "../objects/ui/uiinputfield.h"
#include "../render/fontengine.h"
#include "../services/base/eventservice.h"

class TestScene : public Scene
{
public:
    TestScene(AssetManager* assets, EventService* eventService, FontEngine* fontEngine)
        : Scene(assets), m_EventService(eventService), m_FontEngine(fontEngine) {}

    void OnEnter() override;
    void OnExit() override;
    void Update(float deltatime) override;
    void BuildDebugRenderables(std::vector<DebugRect>& outDebugRects) const override;
    void BuildDebugLines(std::vector<DebugLine>& outDebugLines) const override;
    // void SetInput(const Input& input) override;

private:

    // std::vector<std::unique_ptr<GameObject>> m_GameObjects;
    float m_MoveSpeed = 5.0f;
    PhysicsSystem m_Physics;
    EventService* m_EventService = nullptr;
    FontEngine* m_FontEngine = nullptr;
    
    std::vector<DebugLine> m_TestLines;
    std::vector<DebugRect> m_TestRects;
};
