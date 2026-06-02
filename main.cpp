#include "core/engine.h"
#include "core/serverengine.h"
#include "test/testgame.h"
#include "test/servertestgame.h"

#include <string.h>

int main(int argc, char** argv)
{
    bool isServer = false;
    bool isTest = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--server") == 0) {
            isServer = true;
        }
        if (strcmp(argv[i], "--test") == 0) {
            isTest = true;
        }
    }

    if (isServer) {
        ServerEngine engine;
        ServerTestGame game;
        
        if (!engine.Initialize(&game))
            return -1;
            
        if (isTest) {
            bool passed = game.RunTransitionTest();
            engine.Shutdown();
            return passed ? 0 : -1;
        }

        engine.Run();
        engine.Shutdown();
    } else {
        Engine engine;
        TestGame game;

        if (!engine.Initialize(&game))
            return -1;

        engine.Run();
        engine.Shutdown();
    }

    return 0;
}
