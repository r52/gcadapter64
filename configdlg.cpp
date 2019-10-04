#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>

#include "configdlg.h"
#include "gcadapter.h"
#include "util.h"

ConfigDialog::ConfigDialog(QWidget* parent) : QDialog(parent)
{
    // status
    status               = new QLabel(tr("Adapter Not Detected"));
    statusGroupBox       = new QGroupBox(tr("Status"));
    QHBoxLayout* slayout = new QHBoxLayout;

    slayout->addWidget(status);
    statusGroupBox->setLayout(slayout);

    // settings
    menuGroupBox      = new QGroupBox(tr("Settings"));
    QGridLayout* menu = new QGridLayout;

    QLabel* enabledLabel = new QLabel(tr("Enabled"));
    QLabel* switchLabel  = new QLabel(tr("Use L as Z"));
    QLabel* vcLabel      = new QLabel(tr("Use generic VC deadzone mapping (inaccurate)"));
    vcLabel->setWordWrap(true);

    menu->addWidget(enabledLabel, 0, 1);
    menu->addWidget(switchLabel, 0, 2);
    menu->addWidget(vcLabel, 0, 3);

    for (uint32_t i = 0; i < 4; i++)
    {
        QLabel* cLabel = new QLabel("Controller " + QString::number(i + 1));

        cEnabled[i] = new QCheckBox;
        cEnabled[i]->setChecked(GCAdapter::controller_status[i].enabled);

        cSwap[i] = new QCheckBox;
        cSwap[i]->setChecked(GCAdapter::controller_status[i].l_as_z);

        vcDeadzone[i] = new QCheckBox;
        vcDeadzone[i]->setChecked(GCAdapter::controller_status[i].vcDeadzone);

        menu->addWidget(cLabel, i + 1, 0);
        menu->addWidget(cEnabled[i], i + 1, 1);
        menu->addWidget(cSwap[i], i + 1, 2);
        menu->addWidget(vcDeadzone[i], i + 1, 3);
    }

    menuGroupBox->setLayout(menu);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(saveAndClose()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(statusGroupBox);
    layout->addWidget(menuGroupBox);
    layout->addWidget(buttonBox);

    setLayout(layout);
    setWindowTitle(tr("Settings"));

    if (!GCAdapter::IsDetected())
    {
        if (!GCAdapter::IsDriverDetected())
        {
            status->setText(tr("Driver Not Detected"));
        }
    }
    else
    {
        status->setText(tr("Adapter Detected"));
    }
}

void ConfigDialog::setDetected()
{
    status->setText(tr("Adapter Detected"));
}

void ConfigDialog::saveAndClose()
{
    QSettings settings(SETTINGS_FILE, QSettings::IniFormat);

    for (uint32_t i = 0; i < 4; i++)
    {
        GCAdapter::controller_status[i].enabled    = cEnabled[i]->isChecked();
        GCAdapter::controller_status[i].l_as_z     = cSwap[i]->isChecked();
        GCAdapter::controller_status[i].vcDeadzone = vcDeadzone[i]->isChecked();

        settings.setValue("controller" + QString::number(i) + "/enabled", GCAdapter::controller_status[i].enabled);
        settings.setValue("controller" + QString::number(i) + "/l_as_z", GCAdapter::controller_status[i].l_as_z);
        settings.setValue("controller" + QString::number(i) + "/vcdeadzone", GCAdapter::controller_status[i].vcDeadzone);
    }

    close();
}
