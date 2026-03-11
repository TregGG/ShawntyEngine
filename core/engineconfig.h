#pragma once

// ------------------------------------------------
// Build type
// ------------------------------------------------

#if !defined(ENGINE_DEBUG) && !defined(ENGINE_RELEASE)

    #if defined(_DEBUG) || defined(DEBUG)
        #define ENGINE_DEBUG
    #else
        #define ENGINE_RELEASE
    #endif

#endif


// ------------------------------------------------
// Logging configuration
// ------------------------------------------------

// Default: debug builds log to console + file
#if defined(ENGINE_DEBUG)

    #if !defined(ENGINE_LOG_CONSOLE) && \
        !defined(ENGINE_LOG_FILE) && \
        !defined(ENGINE_LOG_BOTH)

        #define ENGINE_LOG_BOTH

    #endif

#endif