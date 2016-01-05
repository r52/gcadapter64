// Ported from the Dolphin Emulator Project
// Licensed under GPLv2+

#pragma once

#include <cstdint>
#include <atomic>
#include <functional>

struct ControllerStatus
{
    bool enabled;
    bool l_as_z;
};

namespace GCAdapter
{
    extern ControllerStatus controller_status[4];

    void Init();
    void Reset();
    void Setup();
    void Shutdown();
    void SetAdapterCallback(std::function<void(void)> func);
    void StartScanThread();
    void StopScanThread();
    void Input(int chan, uint32_t* command);
    void Output(int chan, uint8_t rumble_command);
    bool IsDetected();
    bool IsDriverDetected();
}

class Flag final
{
public:
    // Declared as explicit since we do not want "= true" to work on a flag
    // object - it should be made explicit that a flag is *not* a normal
    // variable.
    explicit Flag(bool initial_value = false) : m_val(initial_value) {}

    void Set(bool val = true)
    {
        m_val.store(val);
    }

    void Clear()
    {
        Set(false);
    }

    bool IsSet() const
    {
        return m_val.load();
    }

    bool TestAndSet(bool val = true)
    {
        bool expected = !val;
        return m_val.compare_exchange_strong(expected, val);
    }

    bool TestAndClear()
    {
        return TestAndSet(false);
    }

private:
    std::atomic_bool m_val;
};
