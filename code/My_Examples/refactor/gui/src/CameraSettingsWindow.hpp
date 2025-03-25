#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>

class CameraSettingsWindow : public QDialog {
    Q_OBJECT

public:
    explicit CameraSettingsWindow(QWidget* parent = nullptr);
};
