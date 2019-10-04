#include "spec.h"
#include "Controller_1.1.h"
#include "configdlg.h"
#include "gcadapter.h"
#include "util.h"

#include <QApplication>
#include <QMessageBox>
#include <QSettings>

// VC values???
#define DEADZONE 15

#define TEST_VC_DEADZONE(num) (std::abs(num) > DEADZONE) ? (INT8_MAX / (INT8_MAX - DEADZONE)) * (num - (sgn(num) * DEADZONE)) : 0;

static struct GCApp
{
    int          argc    = 1;
    char*        argv[2] = {strdup("dummy"), {}};
    QApplication app{argc, argv};
    ConfigDialog dlg;
}* gc = nullptr;

template <typename T>
int sgn(T val)
{
    return (T(0) < val) - (val < T(0));
}

static void configDlgSetDetected(ConfigDialog* dlg)
{
    QMetaObject::invokeMethod(dlg, "setDetected", Qt::QueuedConnection);
}

static void startGCApp()
{
    if (!gc)
    {
        // Init settings first
        QSettings settings(SETTINGS_FILE, QSettings::IniFormat);

        for (uint32_t i = 0; i < 4; i++)
        {
            GCAdapter::controller_status[i].enabled    = settings.value("controller" + QString::number(i) + "/enabled").toBool();
            GCAdapter::controller_status[i].l_as_z     = settings.value("controller" + QString::number(i) + "/l_as_z").toBool();
            GCAdapter::controller_status[i].vcDeadzone = settings.value("controller" + QString::number(i) + "/vcdeadzone").toBool();
        }

        gc = new GCApp;
        GCAdapter::SetAdapterCallback(std::bind(configDlgSetDetected, &gc->dlg));
    }
}

EXPORT void CALL CloseDLL(void)
{
    GCAdapter::SetAdapterCallback(nullptr);
    GCAdapter::Shutdown();

    if (gc)
    {
        delete gc;
        gc = nullptr;
    }
}

EXPORT void CALL ControllerCommand(int Control, uint8_t* Command)
{
    return;
}

EXPORT void CALL DllAbout(void* hParent)
{
    startGCApp();

    static QString aboutMsg = "Dolphin GC Adapter Plugin<br/>"
                              "<br/>"
                              "Ported from <a href='https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/InputCommon/GCAdapter.cpp'>Dolphin</a><br/>"
                              "<br/>"
                              "Licensed under GPLv2<br/>"
                              "Source code available at <a href='https://github.com/r52/gcadapter64'>GitHub</a>";

    QMessageBox::about(nullptr, "About Dolphin GC Adapter Plugin", aboutMsg);
    gc->app.processEvents();
}

EXPORT void CALL DllConfig(void* hParent)
{
    startGCApp();

    gc->dlg.show();
    gc->app.processEvents();
}

EXPORT void CALL DllTest(void* hParent) {}

EXPORT void CALL GetDllInfo(PLUGIN_INFO* PluginInfo)
{
    PluginInfo->Version = 0x0101;
    PluginInfo->Type    = PLUGIN_TYPE_CONTROLLER;
    strcpy_s(PluginInfo->Name, sizeof(PluginInfo->Name), "Dolphin GC Adapter Plugin");
    return;
}

EXPORT void CALL GetKeys(int32_t Control, BUTTONS* Keys)
{
    GCPadStatus gcpad = GCAdapter::Input(Control);

    if (gcpad.button & PAD_GET_ORIGIN)
        GCAdapter::SetOrigin(Control, gcpad);

    Keys->START_BUTTON = (gcpad.button & PAD_BUTTON_START) != 0;
    Keys->A_BUTTON     = (gcpad.button & PAD_BUTTON_A) != 0;
    Keys->B_BUTTON     = (gcpad.button & PAD_BUTTON_B) != 0;
    Keys->L_DPAD       = (gcpad.button & PAD_BUTTON_LEFT) != 0;
    Keys->U_DPAD       = (gcpad.button & PAD_BUTTON_UP) != 0;
    Keys->R_DPAD       = (gcpad.button & PAD_BUTTON_RIGHT) != 0;
    Keys->D_DPAD       = (gcpad.button & PAD_BUTTON_DOWN) != 0;
    Keys->Z_TRIG       = (gcpad.button & PAD_TRIGGER_Z) != 0;
    Keys->R_TRIG       = (gcpad.button & PAD_TRIGGER_R) != 0;

    if (GCAdapter::controller_status[Control].l_as_z)
    {
        Keys->Z_TRIG = Keys->Z_TRIG | ((gcpad.button & PAD_TRIGGER_L) != 0);
    }
    else
    {
        Keys->L_TRIG = (gcpad.button & PAD_TRIGGER_L) != 0;
    }

    Keys->Y_AXIS = (int8_t) gcpad.stickX - GCAdapter::controller_status[Control].origin.sX;
    Keys->X_AXIS = (int8_t) gcpad.stickY - GCAdapter::controller_status[Control].origin.sY;

    if (GCAdapter::controller_status[Control].vcDeadzone)
    {
        Keys->Y_AXIS = TEST_VC_DEADZONE(Keys->Y_AXIS);
        Keys->X_AXIS = TEST_VC_DEADZONE(Keys->X_AXIS);
    }

    int8_t cstickX = (int8_t) gcpad.substickX - GCAdapter::controller_status[Control].origin.cX;
    int8_t cstickY = (int8_t) gcpad.substickY - GCAdapter::controller_status[Control].origin.cY;

    Keys->L_CBUTTON = (cstickX < -DEADZONE);
    Keys->R_CBUTTON = (cstickX > DEADZONE);

    Keys->D_CBUTTON = (cstickY < -DEADZONE);
    Keys->U_CBUTTON = (cstickY > DEADZONE);
}

EXPORT void CALL InitiateControllers(CONTROL_INFO ControlInfo)
{
    GCAdapter::Init();

    for (uint32_t i = 0; i < 4; i++)
    {
        ControlInfo.Controls[i].Present = GCAdapter::controller_status[i].enabled;
        ControlInfo.Controls[i].RawData = GCAdapter::controller_status[i].enabled;
    }
}

EXPORT void CALL ReadController(int Control, uint8_t* Command)
{
    if (Control == -1)
        return;

    switch (Command[2])
    {
        case RD_RESETCONTROLLER:
        case RD_GETSTATUS:
            // expected: controller gets 1 byte (command), controller sends back 3 bytes
            // should be:	Command[0] == 0x01
            //				Command[1] == 0x03
            Command[3] = RD_GAMEPAD | RD_ABSOLUTE;
            Command[4] = RD_NOEEPROM;

            Command[5] = RD_NOPLUGIN | RD_NOTINITIALIZED;
            break;
        case RD_READKEYS:
        {
            BUTTONS pad;

            uint32_t* val = (uint32_t*) &Command[3]; // alias

            GCPadStatus gcpad = GCAdapter::Input(Control);

            if (gcpad.button & PAD_GET_ORIGIN)
                GCAdapter::SetOrigin(Control, gcpad);

            pad.START_BUTTON = (gcpad.button & PAD_BUTTON_START) != 0;
            pad.A_BUTTON     = (gcpad.button & PAD_BUTTON_A) != 0;
            pad.B_BUTTON     = (gcpad.button & PAD_BUTTON_B) != 0;
            pad.L_DPAD       = (gcpad.button & PAD_BUTTON_LEFT) != 0;
            pad.U_DPAD       = (gcpad.button & PAD_BUTTON_UP) != 0;
            pad.R_DPAD       = (gcpad.button & PAD_BUTTON_RIGHT) != 0;
            pad.D_DPAD       = (gcpad.button & PAD_BUTTON_DOWN) != 0;
            pad.Z_TRIG       = (gcpad.button & PAD_TRIGGER_Z) != 0;
            pad.R_TRIG       = (gcpad.button & PAD_TRIGGER_R) != 0;

            if (GCAdapter::controller_status[Control].l_as_z)
            {
                pad.Z_TRIG = pad.Z_TRIG | ((gcpad.button & PAD_TRIGGER_L) != 0);
            }
            else
            {
                pad.L_TRIG = (gcpad.button & PAD_TRIGGER_L) != 0;
            }

            pad.Y_AXIS = (int8_t) gcpad.stickX - GCAdapter::controller_status[Control].origin.sX;
            pad.X_AXIS = (int8_t) gcpad.stickY - GCAdapter::controller_status[Control].origin.sY;

            if (GCAdapter::controller_status[Control].vcDeadzone)
            {
                pad.Y_AXIS = TEST_VC_DEADZONE(pad.Y_AXIS);
                pad.X_AXIS = TEST_VC_DEADZONE(pad.X_AXIS);
            }

            int8_t cstickX = (int8_t) gcpad.substickX - GCAdapter::controller_status[Control].origin.cX;
            int8_t cstickY = (int8_t) gcpad.substickY - GCAdapter::controller_status[Control].origin.cY;

            pad.L_CBUTTON = (cstickX < -DEADZONE);
            pad.R_CBUTTON = (cstickX > DEADZONE);

            pad.D_CBUTTON = (cstickY < -DEADZONE);
            pad.U_CBUTTON = (cstickY > DEADZONE);

            *val = pad.Value;
            break;
        }
        case RD_READEEPROM:
            break;
        case RD_WRITEEPROM:
            break;
        default:
            Command[1] = Command[1] | RD_ERROR;
            break;
    }
}

EXPORT void CALL RomClosed(void) {}

EXPORT void CALL RomOpen(void) {}
