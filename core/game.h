#ifndef GAME_H
#define GAME_H

#pragma once

class Input;
class OpenGLClass;
class Camera;
class Scene;

#include "../levels/scenemanager.h"
#include "../render/rendermanager.h"
#include "../assets/assetmanager.h"


class Game
{
public:
    virtual ~Game() = default;

    virtual bool OnInit()=0;
    virtual void OnInput(const Input& input) =0;// reference such that it doesnt own the input class
    virtual void OnUpdate(float deltaTime)=0;
    virtual void OnRender()=0;
    virtual void OnShutdown()=0;

    //void SetRenderer(OpenGLClass* renderer) { m_Renderer = renderer; }
    void SetCamera(Camera* camera) { m_Camera = camera; }


    void SetScene(Scene* scene) {m_SceneManager.SetActiveScene(scene);m_RenderManager.BindScene(scene);}

protected:
    //permanent

    SceneManager m_SceneManager;
    RenderManager m_RenderManager;
    AssetManager m_AssetManager;
    //temp
    //OpenGLClass* m_Renderer = nullptr;
    Camera* m_Camera = nullptr;
};


#endif // GAME_H
