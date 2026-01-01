#ifndef GAME_H
#define GAME_H

#pragma once

class Input;

class Game
{
public:
    virtual ~Game() = default;

    virtual bool OnInit()=0;
    virtual void OnInput(const Input& input) =0;// reference such that it doesnt own the input class
    virtual void OnUpdate(float deltaTime)=0;
    virtual void OnRender()=0;
    virtual void OnShutdown()=0;

};


#endif // GAME_H
