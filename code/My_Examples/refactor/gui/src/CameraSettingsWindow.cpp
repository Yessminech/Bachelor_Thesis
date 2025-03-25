#include "CameraSettingsWindow.hpp"

CameraSettingsWindow::CameraSettingsWindow(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Camera Settings");
    setFixedSize(300, 200);

    QFormLayout* layout = new QFormLayout(this);

    QComboBox* cameraSelector = new QComboBox(this);
    for (int i = 0; i < 6; ++i) {
        cameraSelector->addItem(QString("Cam %1").arg(i));
    }

    QComboBox* pixelFormatSelector = new QComboBox(this);
    pixelFormatSelector->addItems({"RGB", "YUV", "Grayscale", "BayerRG", "BayerBG"});

    QLineEdit* heightEdit = new QLineEdit(this);
    QLineEdit* widthEdit = new QLineEdit(this);
    QLineEdit* gainEdit = new QLineEdit(this);
    QPushButton* applyButton = new QPushButton("Apply", this);

    layout->addRow("Camera:", cameraSelector);
    layout->addRow("PixelFormat:", pixelFormatSelector);
    layout->addRow("Height:", heightEdit);
    layout->addRow("Width:", widthEdit);
    layout->addRow("Gain:", gainEdit);
    layout->addWidget(applyButton);
}
