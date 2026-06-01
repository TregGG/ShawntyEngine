#pragma once
#include <string>
#include <unordered_map>

// Data-only struct for scene serialization.
// Holds script metadata loaded from JSON scene files.
// Actual Python execution will be added in Phase 2.
struct ScriptComponent {
    std::string scriptPath;      // e.g. "scripts/player_controller.py"
    std::string className;       // e.g. "PlayerController"
    std::unordered_map<std::string, std::string> properties; // serialized key-value properties
};
