#include "CameraSettingsWindow.hpp"
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>

CameraSettingsWindow::CameraSettingsWindow(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Camera Settings");
    setFixedSize(300, 200);

    QFormLayout* layout = new QFormLayout(this);

    QComboBox* cameraSelector = new QComboBox(this);
    cameraSelector->addItem("All");
    for (int i = 0; i < 6; ++i) {
        cameraSelector->addItem(QString("Cam %1").arg(i));
    }

    QComboBox* pixelFormatSelector = new QComboBox(this);
    pixelFormatSelector->addItems({"RGB", "YUV", "Grayscale", "BayerRG", "BayerBG"});

    QLineEdit* heightEdit = new QLineEdit(this);
    QLineEdit* widthEdit = new QLineEdit(this);
    QLineEdit* gainEdit = new QLineEdit(this);
    QLineEdit* exposureEdit = new QLineEdit(this);
    QPushButton* applyButton = new QPushButton("Apply", this);

    layout->addRow("Camera:", cameraSelector);
    layout->addRow("PixelFormat:", pixelFormatSelector);
    layout->addRow("Exposure Time:", exposureEdit);
    layout->addRow("Height:", heightEdit);
    layout->addRow("Width:", widthEdit);
    layout->addRow("Gain:", gainEdit);
    layout->addWidget(applyButton);

    // Enable/disable controls based on camera selection
    connect(cameraSelector, &QComboBox::currentTextChanged, this, [=](const QString& selection) {
        bool isAll = (selection == "All");
        heightEdit->setEnabled(!isAll);
        widthEdit->setEnabled(!isAll);
        gainEdit->setEnabled(!isAll);
    });
}