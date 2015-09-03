#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QThread>

#include <oplog.h>
#include "configdlg.h"

ConfigDialog::ConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    // status
    status = new QLabel;
    statusGroupBox = new QGroupBox(tr("Status"));
    QHBoxLayout* slayout = new QHBoxLayout;

    slayout->addWidget(status);
    statusGroupBox->setLayout(slayout);

    // settings
    menuGroupBox = new QGroupBox(tr("Settings"));
    QGridLayout* menu = new QGridLayout;

    QLabel* enabledLabel = new QLabel(tr("Enabled"));
    QLabel* switchLabel = new QLabel(tr("Switch L/Z"));

    QLabel* c1Label = new QLabel(tr("Controller 1"));
    QLabel* c2Label = new QLabel(tr("Controller 2"));
    QLabel* c3Label = new QLabel(tr("Controller 3"));
    QLabel* c4Label = new QLabel(tr("Controller 4"));

    QCheckBox* c1Enabled = new QCheckBox();
    QCheckBox* c2Enabled = new QCheckBox();
    QCheckBox* c3Enabled = new QCheckBox();
    QCheckBox* c4Enabled = new QCheckBox();

    QCheckBox* c1Switch = new QCheckBox();
    QCheckBox* c2Switch = new QCheckBox();
    QCheckBox* c3Switch = new QCheckBox();
    QCheckBox* c4Switch = new QCheckBox();

    menu->addWidget(enabledLabel, 0, 1);
    menu->addWidget(switchLabel, 0, 2);

    menu->addWidget(c1Label, 1, 0);
    menu->addWidget(c1Enabled, 1, 1);
    menu->addWidget(c1Switch, 1, 2);

    menu->addWidget(c2Label, 2, 0);
    menu->addWidget(c2Enabled, 2, 1);
    menu->addWidget(c2Switch, 2, 2);

    menu->addWidget(c3Label, 3, 0);
    menu->addWidget(c3Enabled, 3, 1);
    menu->addWidget(c3Switch, 3, 2);

    menu->addWidget(c4Label, 4, 0);
    menu->addWidget(c4Enabled, 4, 1);
    menu->addWidget(c4Switch, 4, 2);

    menuGroupBox->setLayout(menu);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(close()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(statusGroupBox);
    layout->addWidget(menuGroupBox);
    layout->addWidget(buttonBox);

    setLayout(layout);
    setWindowTitle(tr("Settings"));
}

ConfigDialog::~ConfigDialog()
{
}
