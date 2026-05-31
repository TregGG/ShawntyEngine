#include "core/engine.h"
#include "core/game.h"
#include "test/testgame.h"


#include <string.h>

int main(int argc, char** argv)
{
    bool isServer = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--server") == 0) {
            isServer = true;
        }
    }

    Engine engine;
    TestGame game;

    if (!engine.Initialize(&game, isServer))
        return -1;

    engine.Run();
    engine.Shutdown();

    return 0;
}
