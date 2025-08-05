#include "D3DUtil.h"
#include "GameTimer.h"
#include <Windows.h>
GameTimer::GameTimer()
        : m_SecondsPerCount(0.0), m_DeltaTime(-1.0),
    m_BaseTime(0), m_PausedTime(0), m_PrevTime(0), m_CurrentTime(0),
    m_Stopped(false)
    {
        __int64 countPerSec;
        QueryPerformanceFrequency((LARGE_INTEGER*)&countPerSec);
        m_SecondsPerCount = 1.0 / countPerSec;
    }

    float GameTimer::TotalTime() const
    {
        if (m_Stopped)
        {
            return (float)((m_StopTime - m_PausedTime - m_BaseTime) * m_SecondsPerCount);
        }
        return (float)((m_CurrentTime - m_PausedTime - m_BaseTime) * m_SecondsPerCount);
    }

    float GameTimer::DeltaTime() const
    {
        return 0.0;
    }

    void GameTimer::Reset()
    {
        __int64 currTimer;
        QueryPerformanceCounter((LARGE_INTEGER*)&currTimer);

        m_BaseTime = currTimer;
        m_PrevTime = currTimer;
        m_StopTime = 0;
        m_Stopped = false;
    }

    void GameTimer::Start()
    {
        __int64 startTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&startTime);
        if (m_Stopped)
        {
            m_PausedTime += (startTime - m_StopTime);
            m_PrevTime = startTime;
            
            m_StopTime = 0;
            m_Stopped = false;
        }
    }

    void GameTimer::Stop()
    {
        if (!m_Stopped)
        {
            __int64 currTime;
            QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

            m_StopTime = currTime;
            m_Stopped = true;
        }
    }

    void GameTimer::Tick()
    {
        if (m_Stopped)
        {
            m_DeltaTime = 0.0;
            return;
        }
        __int64 currTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
        m_CurrentTime = currTime;

        // 本帧与前一帧的时间差
        m_DeltaTime = (m_CurrentTime - m_PrevTime) * m_SecondsPerCount;

        //准备计算本帧与下一帧的时间差
        m_PrevTime = m_CurrentTime;

        // 时间差为负数。DXSDK中的CDXUTTimer示例注释中提到：如果处理器处于节能模式，或者在
        //计算两帧间隔时间差的过程中切换到另一个处理器（即QueryPerformanceCounter函数的两次调用
        //不处于同一个处理器上），则m_DeltaTime很有可能 < 0
        if (m_DeltaTime < 0.0)
        {
            m_DeltaTime = 0.0;
        }
    }
