#ifndef OPENGLCLASS_H_INCLUDED
#define OPENGLCLASS_H_INCLUDED
class Camera;
class System;

#include<glm/glm.hpp>

class OpenGLClass
{
public:
    OpenGLClass();
    ~OpenGLClass();

    bool Initialize(System* system, int screenWidth, int screenHeight);
    void BeginFrame();
    void EndFrame();
    void Shutdown();

    //Camera functions
    void SetCamera(Camera* camera);
    void DrawQuad(const glm::vec2& position);
protected:
    bool InitializeQuad();
    bool InitializeShaders();
private:

    System* m_system=nullptr;
    int m_screenWidth=0;
    int m_screenHeight=0;

    //Camera
    Camera* m_Camera=nullptr;

    unsigned int m_QuadVAO=0;
    unsigned int m_QuadVBO=0;
    unsigned int m_QuadEBO=0;
    unsigned int m_ShaderProgram=0;
};
#endif // OPENGLCLASS_H_INCLUDED
