#pragma once
#include "../levels/scene.h"
#include <vector>
#include <memory>
#include <glm/vec2.hpp>
#include "../services/base/entityregistry/entityregistry.h"
#include "../services/base/physics/physicssystem.h"
#include "../services/base/raycast.h"
#include "../core/network_data.h"
#include <map>
#include <enet/enet.h>

class GameObject;
class AssetManager;
class Input;

#include "../objects/ui/uipanel.h"
#include "../objects/ui/uitext.h"
#include "../objects/ui/uibutton.h"
#include "../objects/ui/uiinputfield.h"
#include "../render/fontengine.h"
#include "../services/base/eventservice.h"

class NetworkService;
class NetworkControl;

class TestScene : public Scene
{
public:
    TestScene(AssetManager* assets, EventService* eventService, FontEngine* fontEngine, NetworkService* netService, NetworkControl* netControl)
        : Scene(assets), m_EventService(eventService), m_FontEngine(fontEngine), m_NetService(netService), m_NetControl(netControl) {}

    void OnEnter() override;
    void OnExit() override;
    void Update(float deltatime) override;
    void BuildDebugRenderables(std::vector<DebugRect>& outDebugRects) const override;
    void BuildDebugLines(std::vector<DebugLine>& outDebugLines) const override;

private:
    float m_MoveSpeed = 5.0f;
    PhysicsSystem m_Physics;
    EventService* m_EventService = nullptr;
    FontEngine* m_FontEngine = nullptr;
    NetworkService* m_NetService = nullptr;
    NetworkControl* m_NetControl = nullptr;
    UIText* m_StatusText = nullptr;
    bool m_UIHidden = false;
    
    float m_TimeAccumulator = 0.0f;
    
    std::vector<DebugLine> m_TestLines;
    std::vector<DebugRect> m_TestRects;
};
