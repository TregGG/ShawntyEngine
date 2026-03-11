#include "logger.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <thread>
#include <cstdarg>
#include <cstdio>

std::ofstream Logger::m_File;
Logger::Output Logger::m_Output = Logger::Output::Console;


void Logger::Init(Output output)
{
    m_Output = output;

    if (m_Output == Output::File || m_Output == Output::Both)
    {
        m_File.open("logs.txt");
    }
}

void Logger::Shutdown()
{
    if (m_File.is_open())
        m_File.close();
}


std::string Logger::GetTime()
{
    auto t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);

    std::stringstream ss;
    ss << std::put_time(&tm, "%H:%M:%S");

    return ss.str();
}


void Logger::Log(
    const char* className,
    const char* function,
    int line,
    const char* format,
    ...
)
{
    char messageBuffer[1024];

    va_list args;
    va_start(args, format);
    vsnprintf(messageBuffer, sizeof(messageBuffer), format, args);
    va_end(args);

    std::stringstream final;

    final
        << "["
        << GetTime()
        << "]"
        << "[Thread "
        << std::this_thread::get_id()
        << "]"
        << "["
        << className
        << "]"
        << "["
        << function
        << ":"
        << line
        << "] "
        << messageBuffer;

    std::string output = final.str();

    if (m_Output == Output::Console || m_Output == Output::Both)
        std::cout << output << std::endl;

    if ((m_Output == Output::File || m_Output == Output::Both) && m_File.is_open())
        m_File << output << std::endl;
}