#pragma once

#include <string>

// Headless client for checking a server: joins, downloads the content, and prints what happens.
int RunProbe(const std::string& address, int seconds);
