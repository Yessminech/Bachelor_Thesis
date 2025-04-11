#include "CameraSettingsWindow.hpp"
#include "DeviceManager.hpp"
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QDebug>

CameraSettingsWindow::CameraSettingsWindow(QWidget *parent, DeviceManager &deviceManager)
    : QDialog(parent), deviceManager(deviceManager) // ✅ init member
{
    setWindowTitle("Camera Settings");
    setFixedSize(300, 250);

    QFormLayout *layout = new QFormLayout(this);
    QComboBox *cameraSelector = new QComboBox(this);

    auto availableCameras = deviceManager.getAvailableCamerasList();
    if (availableCameras.empty())
    {
        QMessageBox::critical(this, "No Cameras", "No cameras found. Please restart the program.");
        this->close(); // or this->setEnabled(false);
        return;
    }

    for (const auto &cam : availableCameras)
    {
        QString camName = QString::fromStdString(cam->getDisplayName());
        cameraSelector->addItem(camName);
    }
    // Add "All" option last
    cameraSelector->addItem("All");
    cameraSelector->setCurrentIndex(0); // Default: first actual camera

    QComboBox *pixelFormatSelector = new QComboBox(this);
    pixelFormatSelector->addItems({"RGB", "YUV", "Grayscale", "BayerRG", "BayerBG"});

    QLineEdit *exposureEdit = new QLineEdit(this);
    QLineEdit *heightEdit = new QLineEdit(this);
    QLineEdit *widthEdit = new QLineEdit(this);
    QLineEdit *gainEdit = new QLineEdit(this);
    QPushButton *applyButton = new QPushButton("Apply", this);

    layout->addRow("Camera:", cameraSelector);
    layout->addRow("PixelFormat:", pixelFormatSelector);
    layout->addRow("Exposure Time:", exposureEdit);
    layout->addRow("Height:", heightEdit);
    layout->addRow("Width:", widthEdit);
    layout->addRow("Gain:", gainEdit);
    layout->addWidget(applyButton);

    connect(cameraSelector, &QComboBox::currentTextChanged, this, [=](const QString &selection)
            {
        bool isAll = (selection == "All");
    
        heightEdit->clear();
        widthEdit->clear();
        gainEdit->clear();
        heightEdit->setEnabled(!isAll);
        widthEdit->setEnabled(!isAll);
        gainEdit->setEnabled(!isAll);
        heightEdit->setReadOnly(isAll);
        widthEdit->setReadOnly(isAll);
        gainEdit->setReadOnly(isAll);
    
        // PixelFormat, Exposure → only enabled when "All" is selected
        pixelFormatSelector->setEnabled(isAll);
        pixelFormatSelector->setEditable(false); // Optional
        exposureEdit->setEnabled(isAll);
        exposureEdit->setReadOnly(!isAll); });
    // Trigger initial UI state based on default selection
    emit cameraSelector->currentTextChanged(cameraSelector->currentText());

    connect(applyButton, &QPushButton::clicked, this, [=, &deviceManager]()
            {
        QString selectedCam = cameraSelector->currentText();
        QString pixelFormat = pixelFormatSelector->currentText();
        QString exposureVal = exposureEdit->text();
        QString heightVal = heightEdit->text();
        QString widthVal = widthEdit->text();
        QString gainVal = gainEdit->text();

        // TODO: Validate inputs

        qDebug() << "Applying settings to:" << selectedCam;
        qDebug() << "PixelFormat:" << pixelFormat;
        qDebug() << "Exposure:" << exposureVal;
        qDebug() << "Height:" << heightVal;
        qDebug() << "Width:" << widthVal;
        qDebug() << "Gain:" << gainVal;

        if (selectedCam == "All") {
            if (!pixelFormat.isEmpty()) {
                deviceManager.setPixelFormatAll(pixelFormat.toStdString());
            }
            if (!exposureVal.isEmpty()) {
                deviceManager.setExposureTimeAll(exposureVal.toDouble());
            }
        } else {
            pixelFormatSelector->setCurrentIndex(0);
            exposureEdit->clear();

            int camIndex = cameraSelector->currentIndex() - 1;
            if (!gainVal.isEmpty()) {
                deviceManager.setGainAll(gainVal.toFloat());
            }
            if (!heightVal.isEmpty()) {
                deviceManager.setHeightAll(heightVal.toInt());
            }
            if (!widthVal.isEmpty()) {
                deviceManager.setWidthAll(widthVal.toInt());
            }
        }

        QMessageBox::information(this, "Success", "Camera settings applied.");
        this->accept(); });
}
