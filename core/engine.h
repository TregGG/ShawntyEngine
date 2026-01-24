#ifndef ENGINE_H
#define ENGINE_H
#pragma once
class Game;
class System;
class Timer;
class Input;
class OpenGLClass;

class Engine
{
public:
    Engine();
    ~Engine();

    bool Initialize(Game* game);
    void Run();
    void Shutdown();

    void Quit();

private:
    void OnWindowResized(int width, int height);

    bool m_Running=false;

    System* m_System;
    Timer* m_Timer;
    Input* m_Input;
    //Not owned by engine
    Game* m_Game;
    OpenGLClass* m_OpenGL=nullptr;

    int m_LastWidth = 0;
    int m_LastHeight = 0;
};

#endif // ENGINE_H
