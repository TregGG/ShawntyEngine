#include"scenemanger.h"
#include"scene.h"

#include<iostream>
 SceneManager::SceneManager()
{
    m_ActiveScene=nullptr;
}

 SceneManager::~SceneManager()
{

}

Scene* SceneManager::GetActiveScene() const
{
    return m_ActiveScene;
}

void SceneManager::SetInitialScene(Scene* scene)
{
    if(m_ActiveScene)
    {
        std::cerr<<"SceneManager::Failed to set initial Scene:scene already set\n";
        return;
    }
    if(!scene)
    {
        std::cerr <<"SceneManager::Failed to load the scene\n";
        return;
    }
    m_ActiveScene=scene;
    m_ActiveScene->OnEnter();
}

void SceneManager::SetActiveScene(Scene* scene)
{
    if(!scene|| scene==m_ActiveScene)
    {
        std::cerr <<"SceneManager::Failed set Active: no scene to set\n";
        return;
    }
    m_ActiveScene->OnExit();
    m_ActiveScene=scene;
    m_ActiveScene->OnEnter();
}

void SceneManager::Update(float deltaTime)
{
    if(!m_ActiveScene)
    {
        std::cerr<<"SceneManager:: Failed Update: no scene set\n";
        return;
    }
    m_ActiveScene->Update(deltaTime);
}

void SceneManager::Render()
{
    if(!m_ActiveScene)
    {
        std::cerr<<"SceneManager:: Failed Render: no scene set\n";
        return;
    }
 m_ActiveScene->Render();
}

