#include "core/engine.h"
#include "core/serverengine.h"
#include "test/testgame.h"
#include "test/servertestgame.h"

#include <string.h>

int main(int argc, char** argv)
{
    bool isServer = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--server") == 0) {
            isServer = true;
        }
    }

    if (isServer) {
        ServerEngine engine;
        ServerTestGame game;
        
        if (!engine.Initialize(&game))
            return -1;
            
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
