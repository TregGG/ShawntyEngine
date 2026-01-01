#ifndef SCENEMANGER_H_INCLUDED
#define SCENEMANGER_H_INCLUDED
class Scene;
class SceneManager
{
    SceneManager();
    ~SceneManager();

public:

    //----Scene management functions----
    Scene* GetActiveScene() const;
    void SetInitialScene(Scene* scene);
    void SetActiveScene(Scene* scene);

    //----forwards these calls to the active scene----
    //and we are also doing this to help in transitioning from active scene to another
    void Update(float deltaTime);
    void Render();

private:
    Scene* m_ActiveScene=nullptr;

};

#endif // SCENEMANGER_H_INCLUDED
