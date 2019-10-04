// Ported from the Dolphin Emulator Project
// Licensed under GPLv2+

#include <chrono>
#include <mutex>
#include <thread>

#include <libusb-1.0/libusb.h>

#include "gcadapter.h"
#include "util.h"

using namespace std::literals::chrono_literals;

namespace GCAdapter
{
    static bool CheckDeviceAccess(libusb_device* device);
    static void AddGCAdapter(libusb_device* device);
    static void ResetRumbleLockNeeded();
    static void Reset();
    static void Setup();

    enum
    {
        NO_ADAPTER_DETECTED = 0,
        ADAPTER_DETECTED    = 1,
    };

    ControllerStatus controller_status[MAX_N64_CONTROLLERS] = {{false, true, false, {}},
                                                               {false, true, false, {}},
                                                               {false, true, false, {}},
                                                               {false, true, false, {}}};

    static int                   s_status                               = NO_ADAPTER_DETECTED;
    static libusb_device_handle* s_handle                               = nullptr;
    static uint8_t               s_controller_type[MAX_N64_CONTROLLERS] = {ControllerTypes::CONTROLLER_NONE,
                                                             ControllerTypes::CONTROLLER_NONE,
                                                             ControllerTypes::CONTROLLER_NONE,
                                                             ControllerTypes::CONTROLLER_NONE};
    static uint8_t               s_controller_rumble[MAX_N64_CONTROLLERS];

    static std::mutex s_mutex;
    static uint8_t    s_controller_payload[37];
    static uint8_t    s_controller_payload_swap[37];

    static std::atomic<int> s_controller_payload_size = {0};

    static std::thread s_adapter_input_thread;
    static std::thread s_adapter_output_thread;
    static Flag        s_adapter_thread_running;

    static std::mutex  s_init_mutex;
    static std::thread s_adapter_detect_thread;
    static Flag        s_adapter_detect_thread_running;

    static std::function<void(void)> s_detect_callback;

#if defined(__FreeBSD__) && __FreeBSD__ >= 11
    static bool s_libusb_hotplug_enabled = true;
#else
    static bool s_libusb_hotplug_enabled = false;
#endif
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
    static libusb_hotplug_callback_handle s_hotplug_handle;
#endif

    static libusb_context* s_libusb_context = nullptr;
    std::mutex             m_device_list_mutex;

    static uint8_t s_endpoint_in  = 0;
    static uint8_t s_endpoint_out = 0;

    static void Read()
    {
        int payload_size = 0;
        while (s_adapter_thread_running.IsSet())
        {
            libusb_interrupt_transfer(s_handle, s_endpoint_in, s_controller_payload_swap, sizeof(s_controller_payload_swap), &payload_size, 16);

            {
                std::lock_guard<std::mutex> lk(s_mutex);
                std::swap(s_controller_payload_swap, s_controller_payload);
                s_controller_payload_size.store(payload_size);
            }

            std::this_thread::yield();
        }
    }

#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
    static int HotplugCallback(libusb_context* ctx, libusb_device* dev, libusb_hotplug_event event, void* user_data)
    {
        if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)
        {
            if (s_handle == nullptr && CheckDeviceAccess(dev))
            {
                std::lock_guard<std::mutex> lk(s_init_mutex);
                AddGCAdapter(dev);
            }
            else if (s_status < 0 && s_detect_callback != nullptr)
            {
                s_detect_callback();
            }
        }
        else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT)
        {
            if (s_handle != nullptr && libusb_get_device(s_handle) == dev)
                Reset();

            // Reset a potential error status now that the adapter is unplugged
            if (s_status < 0)
            {
                s_status = NO_ADAPTER_DETECTED;
                if (s_detect_callback != nullptr)
                    s_detect_callback();
            }
        }
        return 0;
    }
#endif

    static void ScanThreadFunc()
    {
        LOG_DEBUG(GCAdapter) << "GC Adapter scanning thread started";

#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
#    ifndef __FreeBSD__
        s_libusb_hotplug_enabled = libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) != 0;
#    endif
        if (s_libusb_hotplug_enabled)
        {
            if (libusb_hotplug_register_callback(s_libusb_context,
                                                 (libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
                                                 LIBUSB_HOTPLUG_ENUMERATE,
                                                 0x057e,
                                                 0x0337,
                                                 LIBUSB_HOTPLUG_MATCH_ANY,
                                                 (libusb_hotplug_callback_fn) HotplugCallback,
                                                 nullptr,
                                                 &s_hotplug_handle) != LIBUSB_SUCCESS)
                s_libusb_hotplug_enabled = false;

            if (s_libusb_hotplug_enabled)
                LOG_DEBUG(GCAdapter) << "Using libUSB hotplug detection";
        }
#endif

        if (s_libusb_hotplug_enabled)
            return;

        while (s_adapter_detect_thread_running.IsSet())
        {
            if (s_handle == nullptr)
            {
                std::lock_guard<std::mutex> lk(s_init_mutex);
                Setup();
            }

            std::this_thread::sleep_for(500ms);
        }

        LOG_DEBUG(GCAdapter) << "GC Adapter scanning thread stopped";
    }

    void SetAdapterCallback(std::function<void(void)> func) { s_detect_callback = func; }

    void Init()
    {
        if (s_handle != nullptr)
            return;

        s_status = NO_ADAPTER_DETECTED;

        int ret = libusb_init(&s_libusb_context);

        if (ret)
        {
            LOG_ERROR(GCAdapter) << "libusb_init failed with error: " << ret;
            Shutdown();
            return;
        }

#ifdef _WIN32
        libusb_set_option(s_libusb_context, LIBUSB_OPTION_USE_USBDK);
#endif

        StartScanThread();
    }

    void StartScanThread()
    {
        if (s_adapter_detect_thread_running.IsSet())
            return;
        if (!s_libusb_context)
            return;

        s_adapter_detect_thread_running.Set(true);
        s_adapter_detect_thread = std::thread(ScanThreadFunc);
    }

    void StopScanThread()
    {
        if (s_adapter_detect_thread_running.TestAndClear())
        {
            s_adapter_detect_thread.join();
        }
    }

    static void Setup()
    {
        int prev_status = s_status;

        // Reset the error status in case the adapter gets unplugged
        if (s_status < 0)
            s_status = NO_ADAPTER_DETECTED;

        {
            std::lock_guard<std::mutex> lock{m_device_list_mutex};

            libusb_device** list;
            ssize_t         cnt = libusb_get_device_list(s_libusb_context, &list);

            for (int i = 0; i < MAX_N64_CONTROLLERS; i++)
            {
                s_controller_type[i]   = ControllerTypes::CONTROLLER_NONE;
                s_controller_rumble[i] = 0;
            }

            for (ssize_t d = 0; d < cnt; d++)
            {
                libusb_device* device = list[d];

                if (CheckDeviceAccess(device))
                {
                    // Only connect to a single adapter in case the user has multiple connected
                    AddGCAdapter(device);
                    break;
                }
            }

            libusb_free_device_list(list, 1);
        }

        if (s_status != ADAPTER_DETECTED && prev_status != s_status && s_detect_callback != nullptr)
            s_detect_callback();
    }

    static bool CheckDeviceAccess(libusb_device* device)
    {
        libusb_device_descriptor desc;

        int ret = libusb_get_device_descriptor(device, &desc);
        if (ret)
        {
            // could not acquire the descriptor, no point in trying to use it.
            LOG_ERROR(GCAdapter) << "libusb_get_device_descriptor failed with error: " << ret;
            return false;
        }

        if (desc.idVendor != 0x057e || desc.idProduct != 0x0337)
        {
            // This isn't the device we are looking for.
            return false;
        }

        LOG_DEBUG(GCAdapter) << "Found GC Adapter with Vendor: 0x" << std::hex << desc.idVendor << " Product: 0x" << std::hex << desc.idProduct << " Devnum: 1";

        // In case of failure, capture the libusb error code into the adapter status
        ScopeGuard status_guard([&ret] { s_status = ret; });

        uint8_t bus  = libusb_get_bus_number(device);
        uint8_t port = libusb_get_device_address(device);
        ret          = libusb_open(device, &s_handle);

        if (ret == LIBUSB_ERROR_ACCESS)
        {
            LOG_ERROR(GCAdapter) << "no access to this device: Bus " << bus << " Device " << port << ": ID 0x" << std::hex << desc.idVendor << ":0x" << std::hex
                                 << desc.idProduct << ".";

            return false;
        }

        if (ret)
        {
            LOG_ERROR(GCAdapter) << "libusb_open failed to open device with error = " << ret;
            return false;
        }

        ret = libusb_kernel_driver_active(s_handle, 0);
        if (ret == 1)
        {
            ret = libusb_detach_kernel_driver(s_handle, 0);
            if (ret != 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED)
            {
                LOG_ERROR(GCAdapter) << "libusb_detach_kernel_driver failed with error: " << ret;
            }
        }

        // this split is needed so that we don't avoid claiming the interface when
        // detaching the kernel driver is successful
        if (ret != 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED)
        {
            libusb_close(s_handle);
            s_handle = nullptr;
            return false;
        }

        ret = libusb_claim_interface(s_handle, 0);
        if (ret)
        {
            LOG_ERROR(GCAdapter) << "libusb_claim_interface failed with error: " << ret;
            libusb_close(s_handle);
            s_handle = nullptr;
            return false;
        }

        // Updating the adapter status will be done in AddGCAdapter
        status_guard.Dismiss();

        return true;
    }

    static void AddGCAdapter(libusb_device* device)
    {
        libusb_config_descriptor* config = nullptr;
        libusb_get_config_descriptor(device, 0, &config);
        for (uint8_t ic = 0; ic < config->bNumInterfaces; ic++)
        {
            const libusb_interface* interfaceContainer = &config->interface[ic];
            for (int i = 0; i < interfaceContainer->num_altsetting; i++)
            {
                const libusb_interface_descriptor* interface = &interfaceContainer->altsetting[i];
                for (uint8_t e = 0; e < interface->bNumEndpoints; e++)
                {
                    const libusb_endpoint_descriptor* endpoint = &interface->endpoint[e];
                    if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN)
                        s_endpoint_in = endpoint->bEndpointAddress;
                    else
                        s_endpoint_out = endpoint->bEndpointAddress;
                }
            }
        }

        int           tmp     = 0;
        unsigned char payload = 0x13;
        libusb_interrupt_transfer(s_handle, s_endpoint_out, &payload, sizeof(payload), &tmp, 16);

        s_adapter_thread_running.Set(true);
        s_adapter_input_thread = std::thread(Read);

        s_status = ADAPTER_DETECTED;
        if (s_detect_callback != nullptr)
            s_detect_callback();

        ResetRumbleLockNeeded();
    }

    void Shutdown()
    {
        StopScanThread();
#if defined(LIBUSB_API_VERSION) && LIBUSB_API_VERSION >= 0x01000102
        if (s_libusb_context && s_libusb_hotplug_enabled)
            libusb_hotplug_deregister_callback(s_libusb_context, s_hotplug_handle);
#endif
        Reset();

        s_status = NO_ADAPTER_DETECTED;
    }

    static void Reset()
    {
        std::unique_lock<std::mutex> lock(s_init_mutex, std::defer_lock);
        if (!lock.try_lock())
            return;

        if (s_status != ADAPTER_DETECTED)
            return;

        if (s_adapter_thread_running.TestAndClear())
        {
            s_adapter_input_thread.join();
            s_adapter_output_thread.join();
        }

        for (int i = 0; i < MAX_N64_CONTROLLERS; i++)
            s_controller_type[i] = ControllerTypes::CONTROLLER_NONE;

        s_status = NO_ADAPTER_DETECTED;

        if (s_handle)
        {
            libusb_release_interface(s_handle, 0);
            libusb_close(s_handle);
            s_handle = nullptr;
        }

        if (s_detect_callback != nullptr)
            s_detect_callback();

        LOG_DEBUG(GCAdapter) << "GC Adapter detached";
    }

    GCPadStatus Input(int chan)
    {
        if (s_handle == nullptr || s_status != ADAPTER_DETECTED)
            return {};

        int     payload_size = 0;
        uint8_t controller_payload_copy[37];

        {
            std::lock_guard<std::mutex> lk(s_mutex);
            std::copy(std::begin(s_controller_payload), std::end(s_controller_payload), std::begin(controller_payload_copy));
            payload_size = s_controller_payload_size.load();
        }

        GCPadStatus pad = {};
        if (payload_size != sizeof(controller_payload_copy) || controller_payload_copy[0] != LIBUSB_DT_HID)
        {
            LOG_ERROR(GCAdapter) << "error reading payload (size: " << payload_size << ", type: 0x" << std::hex << controller_payload_copy[0] << ")";
            Reset();
        }
        else
        {
            bool    get_origin = false;
            uint8_t type       = controller_payload_copy[1 + (9 * chan)] >> 4;
            if (type != ControllerTypes::CONTROLLER_NONE && s_controller_type[chan] == ControllerTypes::CONTROLLER_NONE)
            {
                LOG_DEBUG(GCAdapter) << "New device connected to Port " << chan + 1 << " of Type: 0x" << std::hex
                                     << (int) controller_payload_copy[1 + (9 * chan)];
                get_origin = true;
            }

            s_controller_type[chan] = type;

            if (s_controller_type[chan] != ControllerTypes::CONTROLLER_NONE)
            {
                uint8_t b1 = controller_payload_copy[1 + (9 * chan) + 1];
                uint8_t b2 = controller_payload_copy[1 + (9 * chan) + 2];

                if (b1 & (1 << 0))
                    pad.button |= PAD_BUTTON_A;
                if (b1 & (1 << 1))
                    pad.button |= PAD_BUTTON_B;
                if (b1 & (1 << 2))
                    pad.button |= PAD_BUTTON_X;
                if (b1 & (1 << 3))
                    pad.button |= PAD_BUTTON_Y;

                if (b1 & (1 << 4))
                    pad.button |= PAD_BUTTON_LEFT;
                if (b1 & (1 << 5))
                    pad.button |= PAD_BUTTON_RIGHT;
                if (b1 & (1 << 6))
                    pad.button |= PAD_BUTTON_DOWN;
                if (b1 & (1 << 7))
                    pad.button |= PAD_BUTTON_UP;

                if (b2 & (1 << 0))
                    pad.button |= PAD_BUTTON_START;
                if (b2 & (1 << 1))
                    pad.button |= PAD_TRIGGER_Z;
                if (b2 & (1 << 2))
                    pad.button |= PAD_TRIGGER_R;
                if (b2 & (1 << 3))
                    pad.button |= PAD_TRIGGER_L;

                if (get_origin)
                    pad.button |= PAD_GET_ORIGIN;

                pad.stickX       = controller_payload_copy[1 + (9 * chan) + 3];
                pad.stickY       = controller_payload_copy[1 + (9 * chan) + 4];
                pad.substickX    = controller_payload_copy[1 + (9 * chan) + 5];
                pad.substickY    = controller_payload_copy[1 + (9 * chan) + 6];
                pad.triggerLeft  = controller_payload_copy[1 + (9 * chan) + 7];
                pad.triggerRight = controller_payload_copy[1 + (9 * chan) + 8];
            }
        }

        return pad;
    }

    bool DeviceConnected(int chan) { return s_controller_type[chan] != ControllerTypes::CONTROLLER_NONE; }

    void ResetDeviceType(int chan) { s_controller_type[chan] = ControllerTypes::CONTROLLER_NONE; }

    void ResetRumble()
    {
        std::unique_lock<std::mutex> lock(s_init_mutex, std::defer_lock);
        if (!lock.try_lock())
            return;
        ResetRumbleLockNeeded();
    }

    // Needs to be called when s_init_mutex is locked in order to avoid
    // being called while the libusb state is being reset
    static void ResetRumbleLockNeeded()
    {
        if (s_handle == nullptr || s_status != ADAPTER_DETECTED)
        {
            return;
        }

        std::fill(std::begin(s_controller_rumble), std::end(s_controller_rumble), 0);

        unsigned char rumble[5] = {0x11, s_controller_rumble[0], s_controller_rumble[1], s_controller_rumble[2], s_controller_rumble[3]};

        int size = 0;
        libusb_interrupt_transfer(s_handle, s_endpoint_out, rumble, sizeof(rumble), &size, 16);

        LOG_DEBUG(GCAdapter) << "Rumble state reset";
    }

    void SetOrigin(int chan, const GCPadStatus& pad)
    {
        memset(&controller_status[chan].origin, 0, sizeof(controller_status[chan].origin));
        controller_status[chan].origin.sX = pad.stickX;
        controller_status[chan].origin.sY = pad.stickY;
        controller_status[chan].origin.cX = pad.substickX;
        controller_status[chan].origin.cY = pad.substickY;
        controller_status[chan].origin.L  = pad.triggerLeft;
        controller_status[chan].origin.R  = pad.triggerRight;
    }

    bool IsDetected(const char** error_message)
    {
        if (s_status >= 0)
        {
            if (error_message)
                *error_message = nullptr;

            return s_status == ADAPTER_DETECTED;
        }

        if (error_message)
            *error_message = libusb_strerror(static_cast<libusb_error>(s_status));

        return false;
    }
}
