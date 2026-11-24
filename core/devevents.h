#ifndef DEVEVENTS_H
#define DEVEVENTS_H

#pragma once

#include <cstdio>
#include <set>
#include <string>
#include <nlohmann/json.hpp>

// One-line JSON messages on stdout that the cloud backend forwards to the project's editor.
// Unlike ENGINE_LOG these also work in release builds.
namespace DevEvents
{
    inline bool& Enabled() { static bool enabled = false; return enabled; }
    inline std::set<std::string>& Seen() { static std::set<std::string> seen; return seen; }

    inline void Emit(const std::string& level, const std::string& message)
    {
        if (!Enabled()) return;
        const nlohmann::json j = {{"level", level}, {"message", message.substr(0, 4000)}};
        const std::string line = j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        std::fprintf(stdout, "\n@@shawnty %s\n", line.c_str());
        std::fflush(stdout);
    }

    // Script errors can repeat every frame, so each is reported once per reload.
    inline void ScriptError(const std::string& where, const std::string& error)
    {
        if (Seen().size() > 200 || !Seen().insert(where + error).second) return;
        Emit("error", where + ": " + error);
    }

    inline void ResetSeen() { Seen().clear(); }
}

#endif // DEVEVENTS_H
