#ifndef SCENE_H_INCLUDED
#define SCENE_H_INCLUDED

class Scene
{
public:
    virtual  ~Scene() = default;

    virtual void OnEnter() =0;
    virtual void OnExit()=0;
    virtual void Update(float deltatime)=0;
    virtual void Render() =0;
};

#endif // SCENE_H_INCLUDED
