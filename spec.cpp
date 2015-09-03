#include <QApplication>
#include <QThread>
#include <thread>
#include <oplog.h>

#include "gcadapter.h"
#include "spec.h"
#include "Controller #1.1.h"
#include "configdlg.h"

#if defined(QT_STATIC)

#include <QtPlugin>

#ifdef _MSC_VER
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#else
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#endif

#endif


static void configDlgThread()
{
    int argc = 0;
    char* argv = 0;
    QApplication a(argc, &argv);

    ConfigDialog dlg;
    dlg.exec();
    a.exec();
}

EXPORT void CALL CloseDLL(void)
{
    GCAdapter::Shutdown();
}

EXPORT void CALL ControllerCommand(int Control, uint8_t* Command)
{
    return;
}

EXPORT void CALL DllAbout(void* hParent)
{

}

EXPORT void CALL DllConfig(void* hParent)
{
    std::thread dlgThread(configDlgThread);
    dlgThread.join();
}

EXPORT void CALL DllTest(void* hParent)
{

}

EXPORT void CALL GetDllInfo(PLUGIN_INFO* PluginInfo)
{
    PluginInfo->Version = 0x0101;
    PluginInfo->Type = PLUGIN_TYPE_CONTROLLER;
    std::strncpy(PluginInfo->Name, "GC Adapter Test", sizeof(PluginInfo->Name));
    return;
}

EXPORT void CALL GetKeys(int Control, BUTTONS* Keys)
{
    GCAdapter::Input(Control, &Keys->Value);
}

EXPORT void CALL InitiateControllers(CONTROL_INFO* ControlInfo)
{
    GCAdapter::Init();

    ControlInfo->Controls[0].Present = 1;
    ControlInfo->Controls[0].RawData = 1;
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
        GCAdapter::Input(Control, (uint32_t*)&Command[3]);
        break;
    case RD_READEEPROM:
        break;
    case RD_WRITEEPROM:
        break;
    default:
        Command[1] = Command[1] | RD_ERROR;
        break;
    }
}

EXPORT void CALL RomClosed(void)
{

}

EXPORT void CALL RomOpen(void)
{

}
