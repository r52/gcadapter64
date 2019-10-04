// Ported from the Dolphin Emulator Project
// Licensed under GPLv2+

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>

#include "Controller_1.1.h"

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
    bool   enabled;
    bool   l_as_z;
    bool   vcDeadzone;
    Origin origin;
};

enum
{
    PAD_USE_ORIGIN = 0x0080,
    PAD_GET_ORIGIN = 0x2000,
    PAD_ERR_STATUS = 0x8000,
};

enum PadButton
{
    PAD_BUTTON_LEFT  = 0x0001,
    PAD_BUTTON_RIGHT = 0x0002,
    PAD_BUTTON_DOWN  = 0x0004,
    PAD_BUTTON_UP    = 0x0008,
    PAD_TRIGGER_Z    = 0x0010,
    PAD_TRIGGER_R    = 0x0020,
    PAD_TRIGGER_L    = 0x0040,
    PAD_BUTTON_A     = 0x0100,
    PAD_BUTTON_B     = 0x0200,
    PAD_BUTTON_X     = 0x0400,
    PAD_BUTTON_Y     = 0x0800,
    PAD_BUTTON_START = 0x1000,
};

struct GCPadStatus
{
    uint16_t button;       // Or-ed PAD_BUTTON_* and PAD_TRIGGER_* bits
    uint8_t  stickX;       // 0 <= stickX       <= 255
    uint8_t  stickY;       // 0 <= stickY       <= 255
    uint8_t  substickX;    // 0 <= substickX    <= 255
    uint8_t  substickY;    // 0 <= substickY    <= 255
    uint8_t  triggerLeft;  // 0 <= triggerLeft  <= 255
    uint8_t  triggerRight; // 0 <= triggerRight <= 255
    uint8_t  analogA;      // 0 <= analogA      <= 255
    uint8_t  analogB;      // 0 <= analogB      <= 255
    bool     isConnected{true};

    static const uint8_t MAIN_STICK_CENTER_X = 0x80;
    static const uint8_t MAIN_STICK_CENTER_Y = 0x80;
    static const uint8_t MAIN_STICK_RADIUS   = 0x7f;
    static const uint8_t C_STICK_CENTER_X    = 0x80;
    static const uint8_t C_STICK_CENTER_Y    = 0x80;
    static const uint8_t C_STICK_RADIUS      = 0x7f;
};

namespace GCAdapter
{
    enum ControllerTypes
    {
        CONTROLLER_NONE     = 0,
        CONTROLLER_WIRED    = 1,
        CONTROLLER_WIRELESS = 2
    };

    enum
    {
        MAX_N64_CONTROLLERS = 4
    };

    extern ControllerStatus controller_status[MAX_N64_CONTROLLERS];

    void SetOrigin(int chan, const GCPadStatus& pad);

    void        Init();
    void        ResetRumble();
    void        Shutdown();
    void        SetAdapterCallback(std::function<void(void)> func);
    void        StartScanThread();
    void        StopScanThread();
    GCPadStatus Input(int chan);
    bool        IsDetected(const char** error_message);
    bool        DeviceConnected(int chan);
    void        ResetDeviceType(int chan);
}

class Flag final
{
public:
    // Declared as explicit since we do not want "= true" to work on a flag
    // object - it should be made explicit that a flag is *not* a normal
    // variable.
    explicit Flag(bool initial_value = false) : m_val(initial_value) {}

    void Set(bool val = true) { m_val.store(val); }

    void Clear() { Set(false); }

    bool IsSet() const { return m_val.load(); }

    bool TestAndSet(bool val = true)
    {
        bool expected = !val;
        return m_val.compare_exchange_strong(expected, val);
    }

    bool TestAndClear() { return TestAndSet(false); }

private:
    std::atomic_bool m_val;
};

class ScopeGuard final
{
public:
    template <class Callable>
    ScopeGuard(Callable&& finalizer) : m_finalizer(std::forward<Callable>(finalizer))
    {}

    ScopeGuard(ScopeGuard&& other) : m_finalizer(std::move(other.m_finalizer)) { other.m_finalizer = nullptr; }

    ~ScopeGuard() { Exit(); }
    void Dismiss() { m_finalizer = nullptr; }
    void Exit()
    {
        if (m_finalizer)
        {
            m_finalizer(); // must not throw
            m_finalizer = nullptr;
        }
    }

    ScopeGuard(const ScopeGuard&) = delete;

    void operator=(const ScopeGuard&) = delete;

private:
    std::function<void()> m_finalizer;
};
