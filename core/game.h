#ifndef GAME_H
#define GAME_H

#pragma once

class Input;
class Scene;

#include "../levels/scenemanager.h"
#include "../render/rendermanager.h"
#include "../assets/assetmanager.h"
#include "../render/fontengine.h"

class EventService;

class Game
{
public:
    virtual ~Game() = default;

    virtual bool OnInit()=0;
    virtual void OnInput(const Input& input) =0;
    virtual void OnUpdate(float deltaTime)=0;
    virtual void OnRender()=0;
    virtual void OnShutdown()=0;


    void OnResize(int width, int height) {m_RenderManager.OnScreenChange(width, height);}
    void SetScene(Scene* scene) {m_SceneManager.SetActiveScene(scene);m_RenderManager.BindScene(scene);}
    void SetEventService(EventService* es) { m_EventService = es; }

protected:
    SceneManager m_SceneManager;
    RenderManager m_RenderManager;
    AssetManager m_AssetManager;
    FontEngine m_FontEngine;
    EventService* m_EventService = nullptr;
};


#endif // GAME_H
