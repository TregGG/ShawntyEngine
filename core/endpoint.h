#ifndef ENDPOINT_H
#define ENDPOINT_H

#pragma once

#include <cstdlib>
#include <string>

// Parses "host", "host:port" or "host:port/CODE".
inline bool ParseEndpoint(const std::string& text, std::string& host, int& port, std::string& code, int defaultPort = 7777)
{
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string::npos) return false;
    std::string s = text.substr(first, text.find_last_not_of(" \t") - first + 1);

    code.clear();
    if (const auto slash = s.find('/'); slash != std::string::npos) {
        code = s.substr(slash + 1);
        s.resize(slash);
        if (code.empty() || code.size() > 16) return false;
        for (char& c : code) {
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
            else if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return false;
        }
    }

    const auto colon = s.find(':');
    host = s.substr(0, colon);
    port = defaultPort;
    if (host.empty() || host.find(' ') != std::string::npos) return false;
    if (colon == std::string::npos) return true;

    const std::string digits = s.substr(colon + 1);
    if (digits.empty() || digits.size() > 5 || digits.find_first_not_of("0123456789") != std::string::npos) return false;
    port = std::atoi(digits.c_str());
    return port >= 1 && port <= 65535;
}

#endif // ENDPOINT_H
