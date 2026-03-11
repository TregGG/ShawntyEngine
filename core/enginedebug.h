#pragma once

#include "engineconfig.h"
#include <iostream>
#include <cstdlib>

// ------------------------------------------------------------
// Logging
// ------------------------------------------------------------

#if defined(ENGINE_DEBUG)
    #define ENGINE_LOG(x) \
        std::cout << "[ENGINE] " << x << std::endl;

#else

    #define ENGINE_LOG(x)

#endif


#if defined(ENGINE_DEBUG)

    #define ENGINE_ASSERT(condition, message) \
        if (!(condition)) { \
            std::cerr << "ENGINE ASSERT FAILED: " << message << std::endl; \
            std::abort(); \
        }

#else

    #define ENGINE_ASSERT(condition, message)

#endif