#ifndef OPENGLCLASS_H_INCLUDED
#define OPENGLCLASS_H_INCLUDED
class System;

class OpenGLClass
{
public:
    OpenGLClass();
    ~OpenGLClass();

    bool Initialize(System* system, int screenWidth, int screenHeight);
    void BeginFrame();
    void EndFrame();
    void Shutdown();

private:

    System* m_system=nullptr;
    int m_screenWidth=0;
    int m_screenHeight=0;
};
#endif // OPENGLCLASS_H_INCLUDED
