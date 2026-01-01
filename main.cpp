#include "core/engine.h"
#include "core/game.h"
#include"sanitycheck.h"


int main()
{
    Engine engine;
    SanityGame game;


    if (!engine.Initialize(&game))
        return -1;

    engine.Run();
    engine.Shutdown();

    return 0;
}
