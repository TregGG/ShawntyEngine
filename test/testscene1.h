#pragma once
#include "../levels/scene.h"
#include <vector>
#include <memory>
#include <functional>
#include <glm/vec2.hpp>
#include "../services/base/entityregistry/entityregistry.h"
#include "../services/base/physics/physicssystem.h"

class GameObject;
class AssetManager;
class Input;
class NetworkService;
class NetworkControl;
class EventService;
class FontEngine;
class UIText;

class TestScene1 : public Scene
{
public:
    TestScene1(AssetManager* assets, EventService* eventService, FontEngine* fontEngine, NetworkService* netService, NetworkControl* netControl)
        : Scene(assets), m_EventService(eventService), m_FontEngine(fontEngine), m_NetService(netService), m_NetControl(netControl) {}

    void OnEnter() override;
    void OnExit() override;
    void Update(float deltatime) override;
    void BuildDebugRenderables(std::vector<DebugRect>& outDebugRects) const override;
    void BuildDebugLines(std::vector<DebugLine>& outDebugLines) const override;

    std::function<void()> OnAllPlayersInTrigger;

private:
    PhysicsSystem m_Physics;
    EventService* m_EventService = nullptr;
    FontEngine* m_FontEngine = nullptr;
    NetworkService* m_NetService = nullptr;
    NetworkControl* m_NetControl = nullptr;
    UIText* m_StatusText = nullptr;
    bool m_UIHidden = false;

    float m_TimeAccumulator = 0.0f;
    float m_MoveSpeed = 5.0f;

    EntityID m_TriggerID = 0;
};
