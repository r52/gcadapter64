#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QThread>

#include <oplog.h>
#include "configdlg.h"
#include "gcadapter.h"

ConfigDialog::ConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    // status
    status = new QLabel(tr("Adapter Not Detected"));
    statusGroupBox = new QGroupBox(tr("Status"));
    QHBoxLayout* slayout = new QHBoxLayout;

    slayout->addWidget(status);
    statusGroupBox->setLayout(slayout);

    // settings
    menuGroupBox = new QGroupBox(tr("Settings"));
    QGridLayout* menu = new QGridLayout;

    QLabel* enabledLabel = new QLabel(tr("Enabled"));
    QLabel* switchLabel = new QLabel(tr("Swap L/Z trigger"));

    menu->addWidget(enabledLabel, 0, 1);
    menu->addWidget(switchLabel, 0, 2);

    for (uint32_t i = 0; i < 4; i++)
    {
        QLabel* cLabel = new QLabel("Controller " + QString::number(i+1));

        cEnabled[i] = new QCheckBox;
        cEnabled[i]->setChecked(GCAdapter::controller_status[i].enabled);

        cSwap[i] = new QCheckBox;
        cSwap[i]->setChecked(GCAdapter::controller_status[i].lz_swap);

        menu->addWidget(cLabel, i+1, 0);
        menu->addWidget(cEnabled[i], i+1, 1);
        menu->addWidget(cSwap[i], i+1, 2);
    }

    menuGroupBox->setLayout(menu);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(saveAndClose()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    QVBoxLayout *layout = new QVBoxLayout;
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

ConfigDialog::~ConfigDialog()
{
}

void ConfigDialog::setDetected()
{
    status->setText(tr("Adapter Detected"));
}

void ConfigDialog::saveAndClose()
{
    for (uint32_t i = 0; i < 4; i++)
    {
        GCAdapter::controller_status[i].enabled = cEnabled[i]->isChecked();
        GCAdapter::controller_status[i].lz_swap = cSwap[i]->isChecked();
    }

    close();
}
