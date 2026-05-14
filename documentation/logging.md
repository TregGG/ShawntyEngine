# Logging System

The engine includes a built-in logging system to help you debug your game, track events, and catch errors. 

## How to Log Messages

To use the logger in any file, you first need to define the `ENGINE_CLASS` macro at the very top of your file (before including the debug header). This tells the logger which file the message is coming from.

```cpp
#define ENGINE_CLASS "PlayerComponent"
#include "core/enginedebug.h"

// ... inside your code:

void PlayerComponent::Update(float deltaTime) {
    if (health <= 0) {
        ENGINE_LOG("Player died! Score: %d", score);
    }
}
```

The engine uses standard `printf` formatting, meaning you can pass variables into your log messages using `%d` for integers, `%f` for floats, and `%s` for strings.

### Log Levels
There are three levels of severity:

1. **`ENGINE_LOG(...)`**: Standard messages. Use this for general information, like "Level loaded" or "Player spawned".
2. **`ENGINE_WARN(...)`**: Warnings. Use this when something went wrong, but the game can still continue (e.g., "Missing sound file, playing default sound").
3. **`ENGINE_ERROR(...)`**: Errors. Use this for critical failures (e.g., "Failed to compile shader").

### Output Format
Every log message automatically prints a timestamp, the severity level, the class name, the function name, and the exact line number where the log occurred!

Example output:
`[14:30:45][LOG][PlayerComponent][Update:42] Player died! Score: 100`

## Build Configurations

The logger is heavily tied to how you compile the engine:

* **Debug Build (`make BUILD=debug`)**: The logger prints everything to your console AND saves it to a `logs.txt` file in your game directory.
* **Console Build (`make BUILD=console`)**: The logger prints strictly to the console (good for fast iteration).
* **Release Build (`make BUILD=release`)**: The logger is **completely disabled**. All `ENGINE_LOG`, `WARN`, and `ERROR` macros are removed from the code by the compiler, ensuring your final game runs as fast as possible without wasting time formatting strings!
