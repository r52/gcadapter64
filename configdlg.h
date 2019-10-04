#pragma once

#include <QDialog>

#define SETTINGS_FILE "gcadapter.ini"

class QLabel;
class QGroupBox;
class QDialogButtonBox;
class QCheckBox;

class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    ConfigDialog(QWidget* parent = 0);

public slots:
    void setDetected();

private slots:
    void saveAndClose();

private:
    QLabel*           status;
    QGroupBox*        statusGroupBox;
    QGroupBox*        menuGroupBox;
    QDialogButtonBox* buttonBox;

    QCheckBox* cEnabled[4];
    QCheckBox* cSwap[4];
    QCheckBox* vcDeadzone[4];
};
