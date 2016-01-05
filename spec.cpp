#include <QApplication>
#include <QThread>
#include <QSettings>
#include <thread>
#include <oplog.h>
#include <opstrutil.h>

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

static void configDlgSetDetected(ConfigDialog* dlg)
{
    QMetaObject::invokeMethod(dlg, "setDetected", Qt::QueuedConnection);
}

static void configDlgThread()
{
    int argc = 0;
    char* argv = 0;
    QApplication a(argc, &argv);

    ConfigDialog dlg;
    GCAdapter::SetAdapterCallback(std::bind(configDlgSetDetected, &dlg));
    dlg.exec();
    a.exec();
}

EXPORT void CALL CloseDLL(void)
{
    GCAdapter::Shutdown();

    QSettings settings(SETTINGS_FILE, QSettings::IniFormat);

    for (uint32_t i = 0; i < 4; i++)
    {
        settings.setValue("controller" + QString::number(i) + "/enabled", GCAdapter::controller_status[i].enabled);
        settings.setValue("controller" + QString::number(i) + "/l_as_z", GCAdapter::controller_status[i].l_as_z);
    }
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
    if (nullptr == qApp)
    {
        std::thread dlgThread(configDlgThread);
        dlgThread.join();
    }
    else
    {
        ConfigDialog dlg;
        GCAdapter::SetAdapterCallback(std::bind(configDlgSetDetected, &dlg));
        dlg.exec();
    }

    GCAdapter::SetAdapterCallback(nullptr);
}

EXPORT void CALL DllTest(void* hParent)
{

}

EXPORT void CALL GetDllInfo(PLUGIN_INFO* PluginInfo)
{
    PluginInfo->Version = 0x0101;
    PluginInfo->Type = PLUGIN_TYPE_CONTROLLER;
    op::strlcpy(PluginInfo->Name, "GC Adapter Test", sizeof(PluginInfo->Name));
    return;
}

EXPORT void CALL GetKeys(int Control, BUTTONS* Keys)
{
    GCAdapter::Input(Control, &Keys->Value);
}

EXPORT void CALL InitiateControllers(CONTROL_INFO* ControlInfo)
{
    QSettings settings(SETTINGS_FILE, QSettings::IniFormat);

    for (uint32_t i = 0; i < 4; i++)
    {
        GCAdapter::controller_status[i].enabled = settings.value("controller" + QString::number(i) + "/enabled").toBool();
        GCAdapter::controller_status[i].l_as_z = settings.value("controller" + QString::number(i) + "/lz_swap").toBool();
    }

    GCAdapter::Init();

    for (uint32_t i = 0; i < 4; i++)
    {
        ControlInfo->Controls[i].Present = GCAdapter::controller_status[i].enabled;
        ControlInfo->Controls[i].RawData = GCAdapter::controller_status[i].enabled;
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
