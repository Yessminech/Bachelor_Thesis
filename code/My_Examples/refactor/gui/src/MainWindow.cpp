#include "MainWindow.hpp"
#include "CameraSettingsWindow.hpp"
#include "qcustomplot.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QImage>
#include <QPixmap>
#include <QIcon>
#include <QSize>
#include <QTableWidget>
#include <QHeaderView>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), cap(0), savingEnabled(false), settingsWindow(nullptr)
{

    QWidget *central = new QWidget(this);
    QVBoxLayout *rootLayout = new QVBoxLayout(central);

    setupTopBar(rootLayout);
    setupStreamAndInfoArea(rootLayout);
    setupOffsetPlot(rootLayout);

    setCentralWidget(central);
    setWindowTitle("Camera Viewer");
    resize(1100, 900);

    timer = new QTimer(this);
    connectUI();
}

MainWindow::~MainWindow()
{
    stopStream();
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

void MainWindow::setupStreamAndInfoArea(QVBoxLayout *parentLayout)
{
    QHBoxLayout *middleRow = new QHBoxLayout();
    QVBoxLayout *streamLayout = new QVBoxLayout();

    setupVideoFeed(streamLayout);
    setupCameraCheckboxes(streamLayout);

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
                // Implement opening logic for camera i
            }
        } });
    checkLayout->addWidget(openAllButton);
    layout->addLayout(checkLayout);
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
    QTableWidget *offsetTable = new QTableWidget(7, 2);
    offsetTable->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    offsetTable->setFixedHeight(offsetTable->verticalHeader()->defaultSectionSize() * 7 + offsetTable->horizontalHeader()->height());

    offsetTable->setHorizontalHeaderLabels({"Cam ID", "Offset"});
    offsetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    offsetTable->verticalHeader()->setVisible(false);
    offsetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    offsetTable->setSelectionMode(QAbstractItemView::NoSelection);

    for (int row = 0; row < 6; ++row)
    {
        offsetTable->setItem(row, 0, new QTableWidgetItem(QString("Cam %1").arg(row)));
        offsetTable->setItem(row, 1, new QTableWidgetItem("~15.2 ms"));
    }

    QTableWidgetItem *masterItem = new QTableWidgetItem("Master");
    masterItem->setBackground(QColor("lightgray"));
    QFont font;
    font.setBold(true);
    masterItem->setFont(font);
    masterItem->setToolTip("This is the master clock");
    offsetTable->setItem(6, 0, masterItem);
    offsetTable->setItem(6, 1, new QTableWidgetItem("-"));

    layout->addWidget(offsetLabel);
    layout->addWidget(offsetTable);
}

void MainWindow::setupOffsetPlot(QVBoxLayout *layout)
{
    plot = new QCustomPlot(this);
    plot->addGraph();
    plot->graph(0)->setPen(QPen(Qt::blue));
    plot->xAxis->setLabel("Time");
    plot->yAxis->setLabel("Offset");
    plot->xAxis->setRange(0, 100);
    plot->yAxis->setRange(-20, 20);
    plot->setMinimumHeight(200);
    layout->addWidget(plot);
}

void MainWindow::connectUI()
{
    connect(startButton, &QPushButton::clicked, this, &MainWindow::startStream);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::stopStream);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateFrame);
    connect(saveButton, &QPushButton::clicked, this, [=]()
            {
        savingEnabled = saveButton->isChecked();
        saveButton->setText(savingEnabled ? "💾 Save ON" : "💾 Save OFF"); });
}

void MainWindow::startStream()
{
    exposureEdit->setEnabled(false);
    if (!cap.isOpened())
        cap.open(0);
    if (cap.isOpened())
    {
        timer->start(30);
        for (auto *box : cameraCheckboxes)
            box->setEnabled(false);
    }
}

void MainWindow::stopStream()
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
