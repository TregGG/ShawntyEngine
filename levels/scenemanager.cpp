#include"scenemanager.h"
#include"scene.h"
#define ENGINE_CLASS "SceneManager"
#include "../core/enginedebug.h"
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
        ENGINE_ERROR("Failed to set initial scene: scene already set");
        return;
    }
    if(!scene)
    {
        ENGINE_ERROR("Failed to set initial scene: scene is null");
        return;
    }
    m_ActiveScene=scene;
    m_ActiveScene->OnEnter();
}

void SceneManager::SetActiveScene(Scene* scene)
{
    if(m_ActiveScene)
        m_ActiveScene->OnExit();
    if(!scene|| scene==m_ActiveScene)
    {
        ENGINE_WARN("Failed to set active scene: invalid scene");
        m_ActiveScene=nullptr;
        return;
    }
    m_ActiveScene=scene;
    m_ActiveScene->OnEnter();
}

void SceneManager::Update(float deltaTime)
{
    if(!m_ActiveScene)
    {
        ENGINE_WARN("Update skipped: no active scene");
        return;
    }
    m_ActiveScene->Update(deltaTime);
}

// DEPRECATED
//void SceneManager::Render()
//{
//    if(!m_ActiveScene)
//   {
//        std::cerr<<"SceneManager:: Failed Render: no scene set\n";
//        return;
//    }
// m_ActiveScene->Render();
//}



void SceneManager::Shutdown()
{
    if(m_ActiveScene)
    {
        m_ActiveScene->OnExit();
        m_ActiveScene=nullptr;
    }
}