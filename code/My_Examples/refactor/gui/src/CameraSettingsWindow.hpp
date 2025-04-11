#pragma once
#include "DeviceManager.hpp"
#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>

class CameraSettingsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit CameraSettingsWindow(QWidget *parent, DeviceManager &deviceManager);

private:
    DeviceManager &deviceManager;
};
