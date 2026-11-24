#ifndef ENDPOINT_H
#define ENDPOINT_H

#pragma once

#include <cstdlib>
#include <string>

// Parses "host:port" (or just "host", using defaultPort).
inline bool ParseEndpoint(const std::string& text, std::string& host, int& port, int defaultPort = 7777)
{
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string::npos) return false;
    const std::string s = text.substr(first, text.find_last_not_of(" \t") - first + 1);

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
