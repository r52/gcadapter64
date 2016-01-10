// Ported from the Dolphin Emulator Project
// Licensed under GPLv2+

#pragma once

#include <cstdint>
#include <atomic>
#include <functional>

#include "Controller #1.1.h"

struct Origin
{
    uint8_t sX;
    uint8_t sY;
    uint8_t cX;
    uint8_t cY;
    uint8_t L;
    uint8_t R;
};

struct ControllerStatus
{
    bool enabled;
    bool l_as_z;
    bool vcDeadzone;
    Origin origin;
};

namespace GCAdapter
{
    enum PadButton
    {
        PAD_BUTTON_LEFT = 0x0001,
        PAD_BUTTON_RIGHT = 0x0002,
        PAD_BUTTON_DOWN = 0x0004,
        PAD_BUTTON_UP = 0x0008,
        PAD_TRIGGER_Z = 0x0010,
        PAD_TRIGGER_R = 0x0020,
        PAD_TRIGGER_L = 0x0040,
        PAD_BUTTON_A = 0x0100,
        PAD_BUTTON_B = 0x0200,
        PAD_BUTTON_X = 0x0400,
        PAD_BUTTON_Y = 0x0800,
        PAD_BUTTON_START = 0x1000,
    };

    struct GCPadStatus
    {
        uint16_t button;                 // Or-ed PAD_BUTTON_* and PAD_TRIGGER_* bits
        uint8_t  stickX;                 // 0 <= stickX       <= 255
        uint8_t  stickY;                 // 0 <= stickY       <= 255
        uint8_t  substickX;              // 0 <= substickX    <= 255
        uint8_t  substickY;              // 0 <= substickY    <= 255
        uint8_t  triggerLeft;            // 0 <= triggerLeft  <= 255
        uint8_t  triggerRight;           // 0 <= triggerRight <= 255
        uint8_t  analogA;                // 0 <= analogA      <= 255
        uint8_t  analogB;                // 0 <= analogB      <= 255
    };

    extern ControllerStatus controller_status[4];

    void Calibrate(int chan);
    void Init();
    void Reset();
    void Setup();
    void Shutdown();
    void SetAdapterCallback(std::function<void(void)> func);
    void StartScanThread();
    void StopScanThread();
    void Input(int chan, GCPadStatus* pad);
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
