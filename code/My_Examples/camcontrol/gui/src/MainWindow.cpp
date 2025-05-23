/**
 * @brief Library component of the TU Berlin industrial automation framework
 *
 * Copyright (c) 2025  
 * TU Berlin, Institut für Werkzeugmaschinen und Fabrikbetrieb  
 * Fachgebiet Industrielle Automatisierungstechnik  
 * Author: Yessmine Chabchoub  
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.  
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation and/or other materials provided with the distribution.  
 * 3. Neither the name of the copyright holder nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without specific prior written permission.
 *
 * @note
 * DISCLAIMER  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "MainWindow.hpp"
#include "CameraSettingsWindow.hpp"
#include "DeviceManager.hpp"
#include "StreamManager.hpp"
#include "SystemManager.hpp"

#include "GlobalSettings.hpp"
#include "NetworkManager.hpp"
#include "Camera.hpp"
#include <rc_genicam_api/device.h>

#include "qcustomplot.h"
#include <cstdlib> // for std::system
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QImage>
#include <QPixmap>
#include <QIcon>
#include <QSize>
#include <QTableWidget>
#include <QHeaderView>
#include <cmath>
#include <csignal>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QMap>
#include <QDebug>
#include <QtConcurrent/QtConcurrentRun>


SystemManager systemManager = SystemManager();

extern std::atomic<bool> stopStream;
void handleSignal(int signal);

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), savingEnabled(false), settingsWindow(nullptr)
{
    if (systemManager.deviceManager.getAvailableCamerasList().size() > 0)
    {
        availableCameras = systemManager.deviceManager.getAvailableCamerasList();
    }
    else
    {
        qDebug() << "No cameras found.";
    }
    std::signal(SIGINT, handleSignal);

    QWidget *central = new QWidget(this);
    QVBoxLayout *rootLayout = new QVBoxLayout(central);

    setupTopBar(rootLayout);

    // Create a central middle section (stream + tables)
    QHBoxLayout *middleRow = new QHBoxLayout();
    setupStreamAndInfoArea(middleRow);
    rootLayout->addLayout(middleRow);
    plotLayout = new QVBoxLayout();
    plotPlaceholder = new QLabel("PTP plot will appear here", this);
    plotPlaceholder->setAlignment(Qt::AlignCenter);
    plotPlaceholder->setStyleSheet("color: gray; font-style: italic;");
    plotLayout->addWidget(plotPlaceholder);
    rootLayout->addLayout(plotLayout); // Add at bottom of main layout
    setCentralWidget(central);
    setWindowTitle("Camera Viewer");
    resize(1100, 900);

    timer = new QTimer(this);
    connectUI();
}

MainWindow::~MainWindow()
{
    stopStreaming();
    if (settingsWindow)
        delete settingsWindow;
}

void MainWindow::setupTopBar(QVBoxLayout *parentLayout)
{
    QHBoxLayout *topBar = new QHBoxLayout();

    settingsBtn = new QPushButton("⚙️ Settings");
    settingsBtn->setToolTip("Disabled until at least one camera is opened");
    settingsBtn->setEnabled(false); 
    topBar->addWidget(settingsBtn);
    
    startButton = new QPushButton("▶️ Start");
    startButton->setToolTip("Start streaming (disabled until camera is opened)");
    startButton->setEnabled(false);
    topBar->addWidget(startButton);
    
    scheduleButton = new QPushButton("📅 Schedule");
    scheduleButton->setToolTip("Schedule acquisition start (disabled until camera is opened)");
    scheduleButton->setEnabled(false);
    topBar->addWidget(scheduleButton);
    
    stopButton = new QPushButton("⏹ Stop");
    stopButton->setToolTip("Stop streaming (disabled until streaming is started)");
    stopButton->setEnabled(false);
    topBar->addWidget(stopButton);
    
    saveButton = new QPushButton("💾 Save OFF");
    saveButton->setCheckable(true);
    topBar->addWidget(saveButton);
    

    topBar->addStretch();
    parentLayout->addLayout(topBar);
}

void MainWindow::openScheduleDialog()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Schedule Acquisition");
    dialog->setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QLabel *label = new QLabel("Enter delay before acquisition starts (ms):");
    QSpinBox *delayInput = new QSpinBox();
    delayInput->setRange(0, 60000);
    delayInput->setValue(1000);

    QPushButton *saveBtn = new QPushButton("Save");
    layout->addWidget(label);
    layout->addWidget(delayInput);
    layout->addWidget(saveBtn);

    connect(saveBtn, &QPushButton::clicked, this, [=]()
            {
        scheduledDelayMs = delayInput->value();
        dialog->accept();

        if (!scheduleTimer) {
            scheduleTimer = new QTimer(this);
            scheduleTimer->setSingleShot(true);
            connect(scheduleTimer, &QTimer::timeout, this, [=]() {
                QMessageBox::information(this, "Acquisition Started", "Scheduled acquisition has started.");
                startStreaming();  
            });
        }

        scheduleTimer->start(scheduledDelayMs);
        QMessageBox::information(this, "Scheduled", QString("Acquisition will start in %1 ms").arg(scheduledDelayMs)); });

    dialog->exec();
}

void MainWindow::setupStreamAndInfoArea(QHBoxLayout *parentLayout)
{
    QHBoxLayout *middleRow = new QHBoxLayout();
    QVBoxLayout *streamLayout = new QVBoxLayout();

    // Initialize and add the composite stream label
    compositeLabel = new QLabel("Multi-Cam Stream", this);
    compositeLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    compositeLabel->setMinimumSize(480, 270); // just a sane lower bound
    compositeLabel->setStyleSheet("background-color: black;");
    compositeLabel->setAlignment(Qt::AlignCenter);
    streamLayout->addWidget(compositeLabel);

    setupCheckboxUI(streamLayout);

    QVBoxLayout *tableLayout = new QVBoxLayout();
    setupDelayTable(tableLayout);
    setupOffsetTable(tableLayout);

    middleRow->addLayout(streamLayout);
    middleRow->addLayout(tableLayout);
    parentLayout->addLayout(middleRow);
}

void MainWindow::setupCameraCheckboxes(QVBoxLayout *layout)
{
    QHBoxLayout *checkLayout = new QHBoxLayout();
    for (int i = 0; i < 6; ++i)
    {
        QCheckBox *camBox = new QCheckBox(QString("Cam %1").arg(i), this);
        camBox->setEnabled(true);
        cameraCheckboxes.push_back(camBox);
        checkLayout->addWidget(camBox);
    }

    QPushButton *openAllButton = new QPushButton("📷 Open", this);
    openAllButton->setFixedHeight(28);
    connect(openAllButton, &QPushButton::clicked, this, [=]()
            {
        qDebug() << "Opening all checked cameras...";
        for (int i = 0; i < cameraCheckboxes.size(); ++i) {
            if (cameraCheckboxes[i]->isChecked()) {
                qDebug() << "Opening camera" << i;
            }
        } });
    checkLayout->addWidget(openAllButton);
    layout->addLayout(checkLayout);
}

void MainWindow::displayCameraCheckboxes(QVBoxLayout *layout)
{
    if (availableCameras.empty())
    {
        QMessageBox::critical(this, "No Cameras", "No cameras found. Please restart the program.");
        this->setEnabled(false); // Or: this->close();
        return;
    }
    QHBoxLayout *checkLayout = new QHBoxLayout();

    for (const auto &cam : availableCameras)
    {
        QString camName = QString::fromStdString(cam->getID());
        QCheckBox *camBox = new QCheckBox(camName, this);
        camBox->setEnabled(true);

        cameraCheckboxes.push_back(camBox); // store for later
        checkLayout->addWidget(camBox);
    }

    layout->addLayout(checkLayout);
}

void MainWindow::addOpenAllButton(QVBoxLayout *layout)
{
    QPushButton *openAllButton = new QPushButton("📷 Open", this);
    openAllButton->setFixedHeight(28);

    connect(openAllButton, &QPushButton::clicked, this, [this, layout]()
            {
        qDebug() << "Opening all checked cameras...";
        checkedCameraIDs.clear();

        auto camIt = availableCameras.begin();
        for (size_t i = 0; i < cameraCheckboxes.size() && camIt != availableCameras.end(); ++i, ++camIt)
        {
            if (cameraCheckboxes[i]->isChecked())
            {
                std::string camID = (*camIt)->getID();
                checkedCameraIDs.push_back(camID);
                qDebug() << "Selected camera:" << QString::fromStdString(camID);
            }
        }

        if (!checkedCameraIDs.empty())
        {
            systemManager.enumerateOpenCameras(checkedCameraIDs);
            openCamerasList = systemManager.deviceManager.getOpenCameras();
            if (openCamerasList.size() > 1)
            {                
                QtConcurrent::run([this]() {
                    try {
                        systemManager.networkManager.configurePtpSyncFreeRun(this->openCamerasList);
                        /// add a msg 
                    } catch (const std::exception &e) {
                        qDebug() << "Exception in configurePtpSyncFreeRun:" << e.what();
                    } catch (...) {
                        qDebug() << "Unknown exception in configurePtpSyncFreeRun";
                    }
                });    
                plotOffsetCSV(layout, "./output/offset/ptp_offset_history_20250416_171155.csv"); //  CSV should be in the same directory as the executable // ToDo wait fr logs ready // ToDo look for most recent file
           
           }

            // ToDo openAllButton->setEnabled(false);

            settingsBtn->setEnabled(true);
            settingsBtn->setToolTip("Open camera-specific settings");
            
            startButton->setEnabled(true);
            startButton->setToolTip("Start streaming");
            
            scheduleButton->setEnabled(true);
            scheduleButton->setToolTip("Schedule acquisition start");
                                }
        else
        {
            qDebug() << "No cameras selected.";
        } });

    layout->addWidget(openAllButton);
}

void MainWindow::setupCheckboxUI(QVBoxLayout *layout)
{
    displayCameraCheckboxes(layout);
    addOpenAllButton(layout);
}

void MainWindow::setupDelayTable(QVBoxLayout *layout)
{
    layout->setSpacing(0);                  
    layout->setContentsMargins(0, 0, 0, 0); 

    QLabel *delayLabel = new QLabel("Delays");
    delayLabel->setContentsMargins(0, 0, 0, 0);      
    delayLabel->setStyleSheet("margin-bottom: 0px;");
    QTableWidget *delayTable = new QTableWidget(0, 3, this);

    delayTable->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    delayTable->setHorizontalHeaderLabels({"Cam ID", "Packet Delay", "Transmission Delay"});
    delayTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    delayTable->verticalHeader()->setVisible(false);
    delayTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    delayTable->setSelectionMode(QAbstractItemView::NoSelection);

    // layout->addWidget(delayLabel);
    delayTable->setStyleSheet("margin-top: 0px; padding-top: 0px;");

    layout->addWidget(delayTable);

    // Delay loading by 500ms to allow file generation
    QTimer::singleShot(500, this, [=]()
                       { loadDelayTableFromCSV(delayTable, delayLabel); });
}

void MainWindow::loadDelayTableFromCSV(QTableWidget *delayTable, QLabel *delayLabel)
{
    QFile file("./output/bandwidth/bandwidth_delays_20250416_171155.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Failed to open bandwidth_delays.csv.";
        delayLabel->setText("Delays (No data)");
        return;
    }

    QTextStream in(&file);
    QString headerLine = in.readLine(); // Skip header: CameraID,PacketDelayNs,TransmissionDelayNs

    int row = 0;
    delayTable->setRowCount(0); // Clear existing content

    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        QStringList fields = line.split(',');
        if (fields.size() < 3)
            continue;

        delayTable->insertRow(row);
        delayTable->setItem(row, 0, new QTableWidgetItem(fields[0]));
        delayTable->setItem(row, 1, new QTableWidgetItem(fields[1] + " ns"));
        delayTable->setItem(row, 2, new QTableWidgetItem(fields[2] + " ns"));
        row++;
    }

    delayTable->setFixedHeight(
        delayTable->verticalHeader()->defaultSectionSize() * delayTable->rowCount() + delayTable->horizontalHeader()->height());
}

void MainWindow::setupOffsetTable(QVBoxLayout *layout)
{
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    QLabel *offsetLabel = new QLabel("Offsets");
    layout->setSpacing(0); 
    layout->setContentsMargins(0, 0, 0, 0);
    QTableWidget *offsetTable = new QTableWidget(0, 2, this);

    offsetTable->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    offsetTable->setHorizontalHeaderLabels({"Cam ID", "Offset (ns)"});
    offsetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    offsetTable->verticalHeader()->setVisible(false);
    offsetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    offsetTable->setSelectionMode(QAbstractItemView::NoSelection);

    // layout->addWidget(offsetLabel);
    offsetTable->setStyleSheet("margin-top: 0px; padding-top: 0px;");
    layout->addWidget(offsetTable);

    // 🔁 Delay loading by 500ms to allow the file to be generated
    QTimer::singleShot(500, this, [=]()
                       { loadOffsetTableFromCSV(offsetTable, offsetLabel, layout); });
}

void MainWindow::loadOffsetTableFromCSV(QTableWidget *offsetTable, QLabel *offsetLabel, QVBoxLayout *layout)
{
    QFile file("./output/offset/ptp_offset_history_20250416_171155.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Failed to open CSV file for offset table.";
        offsetLabel->setText("Offsets (No data)");
        return;
    }

    QTextStream in(&file);
    QStringList headers = in.readLine().split(',');

    QStringList offsetHeaders;
    for (const QString &h : headers)
    {
        if (h.endsWith("_offset_ns"))
        {
            offsetHeaders.append(h);
        }
    }

    QStringList lastLine;
    while (!in.atEnd())
    {
        QString line = in.readLine();
        if (!line.trimmed().isEmpty())
        {
            lastLine = line.split(',');
        }
    }

    offsetTable->setRowCount(0);
    int masterRow = -1;

    for (int i = 0; i < offsetHeaders.size(); ++i)
    {
        QString fullCamId = offsetHeaders[i];
        fullCamId.replace("_offset_ns", "");

        int colIdx = headers.indexOf(offsetHeaders[i]);
        QString offsetStr = (lastLine.isEmpty() || colIdx >= lastLine.size()) ? "-" : lastLine[colIdx];
        bool ok;
        double offsetValue = offsetStr.toDouble(&ok);

        offsetTable->insertRow(i);

        QTableWidgetItem *idItem = new QTableWidgetItem(fullCamId);
        idItem->setToolTip("Camera ID: " + fullCamId); // Tooltip with full ID

        QTableWidgetItem *offsetItem = new QTableWidgetItem(ok ? QString::number(offsetValue) + " ns" : "-");

        offsetTable->setItem(i, 0, idItem);
        offsetTable->setItem(i, 1, offsetItem);

        if (ok && qFuzzyCompare(offsetValue + 1.0, 1.0))
        {
            masterRow = i;
        }
    }

    // Highlight master row
    if (masterRow >= 0)
    {
        QTableWidgetItem *idItem = offsetTable->item(masterRow, 0);
        QTableWidgetItem *offsetItem = offsetTable->item(masterRow, 1);

        QString originalText = idItem->text();
        idItem->setText("Master (" + originalText + ")");
        idItem->setToolTip("This is the master clock (offset = 0)\nFull ID: " + originalText);
        offsetItem->setToolTip("Offset = 0 ns");

        QFont font;
        font.setBold(true);
        idItem->setFont(font);
        offsetItem->setFont(font);

        idItem->setBackground(QColor(240, 240, 240));
        offsetItem->setBackground(QColor(240, 240, 240));
    }

    offsetTable->setFixedHeight(
        offsetTable->verticalHeader()->defaultSectionSize() * offsetTable->rowCount() + offsetTable->horizontalHeader()->height());
}

void MainWindow::connectUI()
{
    connect(startButton, &QPushButton::clicked, this, &MainWindow::startStreaming);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::stopStreaming);
    connect(saveButton, &QPushButton::clicked, this, [=]()
            {
        savingEnabled = saveButton->isChecked();
        saveButton->setText(savingEnabled ? "💾 Save ON" : "💾 Save OFF"); });
}

void MainWindow::startStreaming()
{
    std::chrono::milliseconds delay(scheduledDelayMs);
    openCamerasList = systemManager.deviceManager.getOpenCameras();
    if (openCamerasList.empty()) {
        std::cerr << "❌ No open cameras found! Aborting stream." << std::endl;
        return;
    }
    std::cout << "📡 Starting streaming with " << openCamerasList.size() << " cameras." << std::endl;

    // exposureEdit->setEnabled(false);
    stopStream = false;
    std::cout << "🔍 Calling streamManager.startPtpSyncFreeRun..." << std::endl;
    std::cout << "⏺ streamManager pointer: " << &systemManager.streamManager << std::endl;

    if (!isOpened)
        saveButton->setEnabled(false);
        systemManager.streamManager.startPtpSyncFreeRun(
        openCamerasList,
        stopStream,
        savingEnabled,
        delay,
        [=](const cv::Mat &compositeFrame)
        {
            if (compositeFrame.empty())
                return;

            cv::Mat rgbFrame;
            cv::cvtColor(compositeFrame, rgbFrame, cv::COLOR_BGR2RGB);

            QImage img(rgbFrame.data, rgbFrame.cols, rgbFrame.rows, rgbFrame.step, QImage::Format_RGB888);
            QPixmap pixmap = QPixmap::fromImage(img.copy());

            QMetaObject::invokeMethod(compositeLabel, [=]()
            {
                compositeLabel->setPixmap(pixmap.scaled(
                    compositeLabel->width(), compositeLabel->height(), 
                    Qt::KeepAspectRatio, Qt::SmoothTransformation));
            });
        });            
        std::cout << "✅ startPtpSyncFreeRun returned." << std::endl;

    isOpened = true;
    if (isOpened)
    {
        timer->start(30);
        for (auto *box : cameraCheckboxes)
            box->setEnabled(false);
    }
    stopButton->setEnabled(true);
stopButton->setToolTip("Stop streaming");

}

void MainWindow::stopStreaming()
{
    if (scheduleTimer && scheduleTimer->isActive())
    {
        scheduleTimer->stop();
        QMessageBox::information(this, "Scheduled Acquisition", "Scheduled acquisition was canceled.");
    }
    stopButton->setEnabled(false);
    stopButton->setToolTip("Disabled until streaming starts");
    
    startButton->setEnabled(false);
    startButton->setToolTip("Start streaming (disabled until camera is opened)");
    //TOdo enable disable are weird
    settingsBtn->setEnabled(true); //Todo false
    settingsBtn->setToolTip("Disabled until at least one camera is opened");
    
    scheduleButton->setEnabled(false);
    scheduleButton->setToolTip("Schedule acquisition start (disabled until camera is opened)");
    
    exposureEdit->setEnabled(true);
    timer->stop();

    // ✅ Actually stop the stream manager
    stopStream = true;
    systemManager.streamManager.stopStreaming(); // just like terminal
    isOpened = false;
    saveButton->setEnabled(true);
    for (auto *box : cameraCheckboxes)
        box->setEnabled(true);
}

void MainWindow::plotOffsetCSV(QVBoxLayout *layout, const QString &csvFilePath)
{
    QFile file(csvFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Failed to open CSV file:" << csvFilePath;
        return;
    }

    QCustomPlot *plot = new QCustomPlot(this);
    plot->legend->setVisible(true);
    plot->legend->setBrush(QBrush(QColor(255, 255, 255, 230)));

    QTextStream in(&file);
    QString headerLine = in.readLine();
    QStringList headers = headerLine.split(',');

    QVector<double> samples;
    QMap<QString, QVector<double>> offsetData;

    // Extract offset columns
    QStringList offsetHeaders;
    for (const QString &h : headers)
    {
        if (h.endsWith("_offset_ns"))
        {
            offsetHeaders.append(h);
            offsetData[h] = QVector<double>();
        }
    }

    // Read each line
    while (!in.atEnd())
    {
        QString line = in.readLine();
        QStringList values = line.split(',');

        if (values.size() < headers.size())
            continue;

        samples.append(values[0].toDouble()); // Sample index

        for (const QString &offsetHeader : offsetHeaders)
        {
            int idx = headers.indexOf(offsetHeader);
            offsetData[offsetHeader].append(values[idx].toDouble());
        }
    }

    // Plot each camera offset line
    int colorIndex = 0;
    for (const QString &offsetHeader : offsetHeaders)
    {
        plot->addGraph();
        QString label = offsetHeader;
        label.replace("_offset_ns", "");
        plot->graph()->setName(label);

        plot->graph()->setData(samples, offsetData[offsetHeader]);
        plot->graph()->setPen(QPen(QColor::fromHsv((colorIndex * 80) % 360, 255, 200), 2));
        colorIndex++;
    }

    // Add threshold line
    const double ptpThreshold = 1000.0; // ns
    QCPItemStraightLine *thresholdLine = new QCPItemStraightLine(plot);
    thresholdLine->point1->setCoords(0, ptpThreshold);
    thresholdLine->point2->setCoords(1, ptpThreshold);
    thresholdLine->setPen(QPen(Qt::red, 1, Qt::DashLine));

    QCPItemText *thresholdLabel = new QCPItemText(plot);
    thresholdLabel->position->setCoords(samples.last(), ptpThreshold + 20);
    thresholdLabel->setText("Threshold");
    thresholdLabel->setColor(Qt::red);
    thresholdLabel->setFont(QFont("Sans", 9));

    // Labels
    plot->xAxis->setLabel("Sample Index");
    plot->yAxis->setLabel("Offset from Master (ns)");
    plot->rescaleAxes();
    plot->setMinimumHeight(300);
    // Clean up previous widgets in the plotLayout
    QLayoutItem *item;
    while ((item = plotLayout->takeAt(0)) != nullptr)
    {
        if (item->widget())
        {
            delete item->widget();
        }
        delete item;
    }
    plotLayout->addWidget(plot);
}