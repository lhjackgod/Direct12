#pragma once

class GameTimer
{
public:
    GameTimer();
    ~GameTimer() = default;

    float TotalTime() const;
    float DeltaTime() const;

    void Reset(); //开始信息循环之前调用
    void Start(); //解除计时器暂停时调用
    void Stop(); //暂停计时器时调用
    void Tick(); //每帧都要调用
private:
    double m_SecondsPerCount;
    double m_DeltaTime;
    __int64 m_BaseTime;
    __int64 m_PausedTime;
    __int64 m_StopTime;
    __int64 m_PrevTime;
    __int64 m_CurrentTime;

    bool m_Stopped;
};
