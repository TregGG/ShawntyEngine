#include "timer.h"

#include<chrono>
#include<algorithm>

using Clock=std::chrono::steady_clock;

Timer::Timer()
{
}

Timer::~Timer()
{
}

void Timer::Start()
{
    auto now = Clock::now();
    m_LastTime = std::chrono::duration<double>(now.time_since_epoch()).count();

    m_CurrentTime = m_LastTime;
    m_DeltaTime   = 0.0f;
    m_TotalTime   = 0.0f;
}

void Timer::Tick()
{
    auto now=Clock::now();
    m_CurrentTime=std::chrono::duration<double>(now.time_since_epoch()).count();

    m_DeltaTime=std::clamp(static_cast<float>(m_CurrentTime-m_LastTime),0.0f,0.1f);
    m_TotalTime += m_DeltaTime;
    m_LastTime = m_CurrentTime;
}


float Timer::GetDeltaTime() const
{
    return m_DeltaTime;
}

float Timer::GetTotalTime() const
{
    return m_TotalTime;
}
