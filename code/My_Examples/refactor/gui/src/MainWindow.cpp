#include "MainWindow.hpp"
#include "CameraSettingsWindow.hpp"
#include "DeviceManager.hpp"
#include "StreamManager.hpp"
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

DeviceManager deviceManager;
StreamManager streamManager;
NetworkManager networkManager;
std::atomic<bool> stopStream(false);
void handleSignal(int signal)
{
    if (signal == SIGINT)
    {
        std::cout << "\nReceived Ctrl+C, stopping streams and cleanup...\n";
        stopStream.store(true);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), cap(0), savingEnabled(false), settingsWindow(nullptr)
{

    if (deviceManager.getAvailableCameras())
    {
        availableCameras = deviceManager.getAvailableCamerasList();
    }
    else
    {
        qDebug() << "No cameras found.";
    }
    std::signal(SIGINT, handleSignal);

    QWidget *central = new QWidget(this);
    QVBoxLayout *rootLayout = new QVBoxLayout(central);

    setupTopBar(rootLayout);
    // setupOffsetPlot(rootLayout);
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
    // topBar->setAlignment(Qt::AlignTop);

    QPushButton *settingsBtn = new QPushButton("⚙️ Settings");
    topBar->addWidget(settingsBtn);
    connect(settingsBtn, &QPushButton::clicked, this, [=]()
            {
        if (!settingsWindow) settingsWindow = new CameraSettingsWindow(this);
        settingsWindow->show();
        settingsWindow->raise();
        settingsWindow->activateWindow(); });

    startButton = new QPushButton("▶️ Start");
    startButton->setToolTip("Start stream");
    topBar->addWidget(startButton);

    stopButton = new QPushButton("⏹ Stop");
    stopButton->setToolTip("Stop stream");
    topBar->addWidget(stopButton);

    saveButton = new QPushButton("💾 Save OFF");
    saveButton->setCheckable(true);
    saveButton->setToolTip("Toggle saving the stream");
    topBar->addWidget(saveButton);
    topBar->addStretch();
    parentLayout->addLayout(topBar);
}

void MainWindow::setupStreamAndInfoArea(QHBoxLayout *parentLayout)
{
    QHBoxLayout *middleRow = new QHBoxLayout();
    QVBoxLayout *streamLayout = new QVBoxLayout();

    setupVideoFeed(streamLayout);
    setupCheckboxUI(streamLayout);
    //  setupCameraCheckboxes(streamLayout)
    QVBoxLayout *tableLayout = new QVBoxLayout();
    setupDelayTable(tableLayout);
    setupOffsetTable(tableLayout);

    middleRow->addLayout(streamLayout);
    middleRow->addLayout(tableLayout);
    parentLayout->addLayout(middleRow);
}

void MainWindow::setupVideoFeed(QVBoxLayout *layout)
{
    videoLabel = new QLabel("Camera feed");
    videoLabel->setFixedSize(640, 480);
    videoLabel->setStyleSheet("background-color: black;");
    videoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(videoLabel);
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
        return;
    }

    QHBoxLayout *checkLayout = new QHBoxLayout();

    for (const auto &cam : availableCameras)
    {
        QString camName = QString::fromStdString(cam->getDisplayName());
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
                qDebug() << "Selected camera ID:" << QString::fromStdString(camID);
            }
        }

        if (!checkedCameraIDs.empty())
        {
            // deviceManager.openCameras(checkedCameraIDs);
            // networkManager.enablePtp(openCamerasList); 
            // networkManager.configureMultiCamerasNetwork(openCamerasList);
            // networkManager.monitorPtpStatus(openCamerasList);
            // networkManager.monitorPtpOffset(openCamerasList);
            plotOffsetCSV(layout, "./../../ptp_offset_history.csv"); //  CSV should be in the same directory as the executable
            qDebug() << "Requested to open" << checkedCameraIDs.size() << "camera(s).";
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
    QLabel *delayLabel = new QLabel("Delays");
    QTableWidget *delayTable = new QTableWidget(6, 3);
    delayTable->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    delayTable->setFixedHeight(delayTable->verticalHeader()->defaultSectionSize() * 6 + delayTable->horizontalHeader()->height());

    delayTable->setHorizontalHeaderLabels({"Cam ID", "Packet Delay", "Transmission Delay"});
    delayTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    delayTable->verticalHeader()->setVisible(false);
    delayTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    delayTable->setSelectionMode(QAbstractItemView::NoSelection);

    for (int row = 0; row < 6; ++row)
    {
        delayTable->setItem(row, 0, new QTableWidgetItem(QString("Cam %1").arg(row)));
        delayTable->setItem(row, 1, new QTableWidgetItem("~123 ms"));
        delayTable->setItem(row, 2, new QTableWidgetItem("~456 ms"));
    }

    layout->addWidget(delayLabel);
    layout->addWidget(delayTable);
}

void MainWindow::setupOffsetTable(QVBoxLayout *layout)
{
    QLabel *offsetLabel = new QLabel("Offsets");
    QTableWidget *offsetTable = new QTableWidget(0, 2, this);

    offsetTable->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    offsetTable->setHorizontalHeaderLabels({"Cam ID", "Offset (ns)"});
    offsetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    offsetTable->verticalHeader()->setVisible(false);
    offsetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    offsetTable->setSelectionMode(QAbstractItemView::NoSelection);

    layout->addWidget(offsetLabel);
    layout->addWidget(offsetTable);

    // 🔁 Delay loading by 500ms to allow the file to be generated
    QTimer::singleShot(500, this, [=]()
                       { loadOffsetTableFromCSV(offsetTable, offsetLabel, layout); });
}

void MainWindow::loadOffsetTableFromCSV(QTableWidget* offsetTable, QLabel* offsetLabel, QVBoxLayout* layout)
{
    QFile file("./../../ptp_offset_history.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open CSV file for offset table.";
        offsetLabel->setText("Offsets (No data)");
        return;
    }

    QTextStream in(&file);
    QStringList headers = in.readLine().split(',');

    QStringList offsetHeaders;
    for (const QString &h : headers) {
        if (h.endsWith("_offset_ns")) {
            offsetHeaders.append(h);
        }
    }

    QStringList lastLine;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!line.trimmed().isEmpty()) {
            lastLine = line.split(',');
        }
    }

    offsetTable->setRowCount(0);
    int masterRow = -1;

    for (int i = 0; i < offsetHeaders.size(); ++i) {
        QString fullCamId = offsetHeaders[i];
        fullCamId.replace("_offset_ns", "");

        int colIdx = headers.indexOf(offsetHeaders[i]);
        QString offsetStr = (lastLine.isEmpty() || colIdx >= lastLine.size()) ? "-" : lastLine[colIdx];
        bool ok;
        double offsetValue = offsetStr.toDouble(&ok);

        offsetTable->insertRow(i);

        QTableWidgetItem* idItem = new QTableWidgetItem(fullCamId);
        idItem->setToolTip("Camera ID: " + fullCamId);  // Tooltip with full ID

        QTableWidgetItem* offsetItem = new QTableWidgetItem(ok ? QString::number(offsetValue) + " ns" : "-");

        offsetTable->setItem(i, 0, idItem);
        offsetTable->setItem(i, 1, offsetItem);

        if (ok && qFuzzyCompare(offsetValue + 1.0, 1.0)) {
            masterRow = i;
        }
    }

    // Highlight master row
    if (masterRow >= 0) {
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
        offsetTable->verticalHeader()->defaultSectionSize() * offsetTable->rowCount()
        + offsetTable->horizontalHeader()->height());
}


void MainWindow::connectUI()
{
    connect(startButton, &QPushButton::clicked, this, &MainWindow::startStreaming);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::stopStreaming);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateFrame);
    connect(saveButton, &QPushButton::clicked, this, [=]()
            {
        savingEnabled = saveButton->isChecked();
        saveButton->setText(savingEnabled ? "💾 Save ON" : "💾 Save OFF"); });
}

void MainWindow::startStreaming()
{
    openCamerasList = deviceManager.getopenCameras();
    exposureEdit->setEnabled(false);
    if (!isOpened)
        streamManager.startSyncFreeRun(openCamerasList, stopStream, savingEnabled);
    isOpened = true;
    if (isOpened)
    {
        timer->start(30);
        for (auto *box : cameraCheckboxes)
            box->setEnabled(false);
    }
}

void MainWindow::stopStreaming()
{
    exposureEdit->setEnabled(true);
    timer->stop();
    if (cap.isOpened())
        cap.release();
    videoLabel->clear();
    for (auto *box : cameraCheckboxes)
        box->setEnabled(true);
}

void MainWindow::updateFrame()
{
    static int t = 0;
    double y = 10 * sin(0.1 * t);
    plot->graph(0)->addData(t, y);
    plot->graph(0)->data()->removeBefore(t - 100);
    plot->xAxis->setRange(t - 100, t);
    plot->replot();
    t++;

    cv::Mat frame;
    cap >> frame;
    if (frame.empty())
        return;

    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
    QImage image(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
    videoLabel->setPixmap(QPixmap::fromImage(image).scaled(videoLabel->size(), Qt::KeepAspectRatio));

    if (savingEnabled)
    {
        static int counter = 0;
        std::string filename = "frame_" + std::to_string(counter++) + ".png";
        cv::imwrite(filename, frame);
    }
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