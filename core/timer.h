#ifndef TIMER_H_INCLUDED
#define TIMER_H_INCLUDED

#pragma once

class Timer
{

public:
    Timer();
    ~Timer();

    void Start();
    void Tick();
    float GetDeltaTime() const;
    float GetTotalTime() const;
private:
    double m_LastTime = 0.0;
    double m_CurrentTime = 0.0;
    float  m_DeltaTime = 0.0f;
    float  m_TotalTime = 0.0f;
};

#endif // TIMER_H_INCLUDED
