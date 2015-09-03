#pragma once

#include <QDialog>

class QLabel;
class QGroupBox;
class QDialogButtonBox;

class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    ConfigDialog(QWidget* parent = 0);
    ~ConfigDialog();

private:
    QLabel* status;
    QGroupBox* statusGroupBox;
    QGroupBox* menuGroupBox;
    QDialogButtonBox* buttonBox;
};
