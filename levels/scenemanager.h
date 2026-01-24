#ifndef SCENEMANGER_H_INCLUDED
#define SCENEMANGER_H_INCLUDED
class Scene;
class SceneManager
{
public:

    SceneManager();
    ~SceneManager();


    //----Scene management functions----
    Scene* GetActiveScene() const;
    void SetInitialScene(Scene* scene);
    void SetActiveScene(Scene* scene);

    //----forwards these calls to the active scene----
    //and we are also doing this to help in transitioning from active scene to another
    void Update(float deltaTime);
    //void Render();
    void Shutdown();

private:
    Scene* m_ActiveScene=nullptr;

};

#endif // SCENEMANGER_H_INCLUDED
