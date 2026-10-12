#pragma once

#include <chrono>
#include <thread>

// Millisecond OS sleeps can overshoot short frame intervals. Sleep most of the
// interval, then yield until the precise deadline instead of rounding it up.
inline void WaitForFrameDeadline(std::chrono::steady_clock::time_point deadline)
{
    constexpr auto precisionWindow = std::chrono::milliseconds(2);
    for (;;)
    {
        const auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero())
            return;

        const auto sleepTime = std::chrono::duration_cast<std::chrono::milliseconds>(remaining - precisionWindow);
        if (sleepTime.count() > 0)
            std::this_thread::sleep_for(sleepTime);
        else
            std::this_thread::yield();
    }
}