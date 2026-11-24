#ifndef PROJECTPATHS_H
#define PROJECTPATHS_H

#pragma once

#include <string>

// Where the running project's data lives. Scenes and scripts refer to files as
// "test_compiled/...", so that prefix is mapped onto the selected project root.
namespace ProjectPaths
{
    inline const std::string kDefaultRoot = "test_compiled";

    inline std::string& RootRef()
    {
        static std::string root = kDefaultRoot;
        return root;
    }

    inline const std::string& Root() { return RootRef(); }

    inline void SetRoot(std::string root)
    {
        while (root.size() > 1 && root.back() == '/') root.pop_back();
        RootRef() = std::move(root);
    }

    inline bool IsDefault() { return Root() == kDefaultRoot; }

    // The path relative to the project root, as sent over the network
    inline std::string MakeRelative(const std::string& path)
    {
        const std::string prefix = Root() + "/";
        return path.compare(0, prefix.size(), prefix) == 0 ? path.substr(prefix.size()) : path;
    }

    inline std::string Resolve(const std::string& path)
    {
        if (IsDefault() || path.empty() || path.front() == '/') return path;
        const std::string& root = Root();
        if (path.compare(0, root.size() + 1, root + "/") == 0) return path;
        const std::string prefix = kDefaultRoot + "/";
        if (path.compare(0, prefix.size(), prefix) == 0) return root + "/" + path.substr(prefix.size());
        return root + "/" + path;
    }
}

#endif // PROJECTPATHS_H
