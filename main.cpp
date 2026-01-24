#include "core/engine.h"
#include "core/game.h"
#include "test/testgame.h"


int main()
{
    Engine engine;
    TestGame game;

    if (!engine.Initialize(&game))
        return -1;

    engine.Run();
    engine.Shutdown();

    return 0;
}
