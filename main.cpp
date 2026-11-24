#include "core/engine.h"
#include "core/serverengine.h"
#include "core/projectpaths.h"
#include "core/devevents.h"
#include "core/probe.h"
#include "services/networkcontrol.h"
#include "levels/datadrivenscene.h"
#include "test/testgame.h"
#include "test/servertestgame.h"

#include <cstdio>
#include <cstdlib>
#include <string.h>

static bool ParsePort(const char* text, int& out)
{
    char* end = nullptr;
    long value = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 1 || value > 65535) return false;
    out = static_cast<int>(value);
    return true;
}

int main(int argc, char** argv)
{
    bool isServer = false;
    bool isTest = false;
    std::string connectTo;
    std::string probe;
    int probeSeconds = 5;
    int port = 7777;
    int reloadPort = 9090;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--server") == 0) {
            isServer = true;
        }
        if (strcmp(argv[i], "--test") == 0) {
            isTest = true;
        }
        if (strcmp(argv[i], "--net-stats") == 0) NetworkControl::SetPrintStats(true);
        if (strcmp(argv[i], "--dev-events") == 0) DevEvents::Enabled() = true;
        const bool isConnect = strcmp(argv[i], "--connect") == 0;
        const bool isProbe = strcmp(argv[i], "--probe") == 0;
        const bool isSeconds = strcmp(argv[i], "--seconds") == 0;
        const bool isProjectDir = strcmp(argv[i], "--project-dir") == 0;
        const bool isPort = strcmp(argv[i], "--port") == 0;
        const bool isReloadPort = strcmp(argv[i], "--reload-port") == 0;
        if (isProjectDir || isPort || isReloadPort || isConnect || isProbe || isSeconds) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing value for %s\n", argv[i]);
                return 2;
            }
            const char* value = argv[++i];
            if (isProjectDir) {
                ProjectPaths::SetRoot(value);
            } else if (isConnect) {
                connectTo = value;
            } else if (isProbe) {
                probe = value;
            } else if (isSeconds) {
                probeSeconds = std::atoi(value);
            } else if (!ParsePort(value, isPort ? port : reloadPort)) {
                fprintf(stderr, "Invalid port: %s\n", value);
                return 2;
            }
        }
    }

    DataDrivenScene::SetUdpPort(reloadPort);
    if (!probe.empty()) return RunProbe(probe, probeSeconds);

    if (isServer) {
        ServerEngine engine;
        ServerTestGame game;
        
        if (!engine.Initialize(&game, port, !ProjectPaths::IsDefault()))
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
        game.SetAutoConnect(connectTo);

        if (!engine.Initialize(&game))
            return -1;

        engine.Run();
        engine.Shutdown();
    }

    return 0;
}
