#pragma once

#include "engineconfig.h"
#include "logger.h"
#include <cstdlib>

// ------------------------------------------------------------
// Logging
// ------------------------------------------------------------

#if defined(ENGINE_RELEASE)

    #define ENGINE_LOG(...)
    #define ENGINE_WARN(...)
    #define ENGINE_ERROR(...)

#else

    #ifndef ENGINE_CLASS
        #define ENGINE_CLASS "ENGINE"
    #endif

    #define ENGINE_LOG(format, ...) \
        Logger::Log(Logger::Level::Log, ENGINE_CLASS, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

    #define ENGINE_WARN(format, ...) \
        Logger::Log(Logger::Level::Warn, ENGINE_CLASS, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

    #define ENGINE_ERROR(format, ...) \
        Logger::Log(Logger::Level::Error, ENGINE_CLASS, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

#endif


// ------------------------------------------------------------
// Assertions
// ------------------------------------------------------------

#if defined(ENGINE_DEBUG)

    #define ENGINE_ASSERT(condition, message) \
        if (!(condition)) { \
            Logger::Log(ENGINE_CLASS, __FUNCTION__, __LINE__, message); \
            std::abort(); \
        }

#else

    #define ENGINE_ASSERT(condition, message)

#endif