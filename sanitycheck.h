#ifndef SANITYCHECK_H_INCLUDED
#define SANITYCHECK_H_INCLUDED

#pragma once
#include "core/game.h"

class SanityGame : public Game
{
public:
    virtual bool OnInit() override;
    virtual void OnInput(const Input& input) override;
    virtual void OnUpdate(float deltaTime) override;
    virtual void OnRender() override;
    virtual void OnShutdown() override;
};


#endif // SANITYCHECK_H_INCLUDED
