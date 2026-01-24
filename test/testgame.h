#pragma once

#include "../core/game.h"
#include "../levels/scenemanager.h"
#include "../render/rendermanager.h"
#include "../assets/assetmanager.h"

class Scene;

class TestGame : public Game
{
public:
    bool OnInit() override;
    void OnInput(const Input& input) override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnShutdown() override;

private:
    Scene* m_TestScene = nullptr;
};