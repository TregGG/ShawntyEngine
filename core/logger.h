#pragma once

#include <fstream>
#include <string>

class Logger
{
public:

    enum class Output
    {
        Console,
        File,
        Both
    };

    static void Init(Output output = Output::Console);
    static void Shutdown();

    static void Log(
        const char* className,
        const char* function,
        int line,
        const char* format,
        ...
    );

private:

    static std::ofstream m_File;
    static Output m_Output;

    static std::string GetTime();
};