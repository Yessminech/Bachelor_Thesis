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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), cap(0), savingEnabled(false), settingsWindow(nullptr) {

    QWidget* central = new QWidget(this);
    QVBoxLayout* rootLayout = new QVBoxLayout(central);

    // --- Top Button Bar ---
    QHBoxLayout* topBar = new QHBoxLayout();
    topBar->setAlignment(Qt::AlignTop);

    startButton = new QPushButton();
    startButton->setIcon(QIcon("icons/play.png"));
    startButton->setIconSize(QSize(32, 32));
    startButton->setToolTip("Start stream");
    topBar->addWidget(startButton);

    stopButton = new QPushButton();
    stopButton->setIcon(QIcon("icons/stop.png"));
    stopButton->setIconSize(QSize(32, 32));
    stopButton->setToolTip("Stop stream");
    topBar->addWidget(stopButton);

    topBar->addStretch();
    rootLayout->addLayout(topBar);

    // --- Stream & Info Area ---
    QHBoxLayout* middleRow = new QHBoxLayout();

    QVBoxLayout* streamLayout = new QVBoxLayout();

    videoLabel = new QLabel("Camera feed");
    videoLabel->setFixedSize(640, 480);
    videoLabel->setStyleSheet("background-color: black;");
    videoLabel->setAlignment(Qt::AlignCenter);
    streamLayout->addWidget(videoLabel);

    QHBoxLayout* checkLayout = new QHBoxLayout();
    for (int i = 0; i < 6; ++i) {
        QCheckBox* camBox = new QCheckBox(QString("Cam %1").arg(i), this);
        camBox->setEnabled(true);
        cameraCheckboxes.push_back(camBox);
        checkLayout->addWidget(camBox);
    }
    streamLayout->addLayout(checkLayout);

    QHBoxLayout* controlsLayout = new QHBoxLayout();
    QPushButton* settingsBtn = new QPushButton("⚙️ Settings");
    controlsLayout->addWidget(settingsBtn);

    exposureEdit = new QLineEdit(this);
    exposureEdit->setPlaceholderText("Exposure (ms)");
    exposureEdit->setFixedWidth(120);
    exposureEdit->setToolTip("Set exposure before starting the stream");
    controlsLayout->addWidget(exposureEdit);

    controlsLayout->addStretch();
    saveButton = new QPushButton("💾 Save OFF");
    saveButton->setCheckable(true);
    saveButton->setToolTip("Toggle saving the stream");
    controlsLayout->addWidget(saveButton);
    streamLayout->addLayout(controlsLayout);

    middleRow->addLayout(streamLayout);

    // --- Delay and Offset Tables ---
    QVBoxLayout* tableLayout = new QVBoxLayout();

    QLabel* delayLabel = new QLabel("Delays");
    QTableWidget* delayTable = new QTableWidget(6, 3);
    delayTable->setHorizontalHeaderLabels({"Cam ID", "Packet Delay", "Transmission Delay"});
    delayTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    delayTable->verticalHeader()->setVisible(false);
    tableLayout->addWidget(delayLabel);
    tableLayout->addWidget(delayTable);

    QLabel* offsetLabel = new QLabel("Offsets");
    QTableWidget* offsetTable = new QTableWidget(7, 2);
    offsetTable->setHorizontalHeaderLabels({"Cam ID", "Offset"});
    offsetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    offsetTable->verticalHeader()->setVisible(false);

    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 2; ++col) {
            QTableWidgetItem* item = new QTableWidgetItem(row == 6 ? (col == 0 ? "Master" : "-") : "-");
            if (row == 6) {
                item->setBackground(QColor("lightgray"));
                QFont font;
                font.setBold(true);
                item->setFont(font);
                if (col == 0) item->setToolTip("This is the master clock");
            }
            offsetTable->setItem(row, col, item);
        }
    }
    tableLayout->addStretch(); // 👈 Add this BEFORE adding labels/tables
    tableLayout->addWidget(offsetLabel);
    tableLayout->addWidget(offsetTable);
    tableLayout->addStretch(); // 👈 Move this AFTER adding labels/tables

    middleRow->addLayout(tableLayout);
    rootLayout->addLayout(middleRow);

    // --- Offset Plot ---
    plot = new QCustomPlot(this);
    plot->addGraph();
    plot->graph(0)->setPen(QPen(Qt::blue));
    plot->xAxis->setLabel("Time");
    plot->yAxis->setLabel("Offset");
    plot->xAxis->setRange(0, 100);
    plot->yAxis->setRange(-20, 20);
    plot->setMinimumHeight(200);
    rootLayout->addWidget(plot);

    setCentralWidget(central);
    setWindowTitle("Camera Viewer");
    resize(1100, 900);

    timer = new QTimer(this);

    connect(startButton, &QPushButton::clicked, this, &MainWindow::startStream);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::stopStream);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateFrame);
    connect(saveButton, &QPushButton::clicked, [=]() {
        savingEnabled = saveButton->isChecked();
        saveButton->setText(savingEnabled ? "💾 Save ON" : "💾 Save OFF");
    });
    connect(settingsBtn, &QPushButton::clicked, this, [=]() {
        if (!settingsWindow) {
            settingsWindow = new CameraSettingsWindow(this);
        }
        settingsWindow->show();
        settingsWindow->raise();
        settingsWindow->activateWindow();
    });
}


MainWindow::~MainWindow()
{
    stopStream();
    if (settingsWindow)
    {
        delete settingsWindow;
    }
}

void MainWindow::startStream()
{
    exposureEdit->setEnabled(false);

    if (!cap.isOpened())
    {
        cap.open(0);
    }
    if (cap.isOpened())
    {
        timer->start(30);
        for (auto *box : cameraCheckboxes)
        {
            box->setEnabled(false);
        }
    }
}

void MainWindow::stopStream()
{
    exposureEdit->setEnabled(true);

    timer->stop();
    if (cap.isOpened())
    {
        cap.release();
    }
    videoLabel->clear();
    for (auto *box : cameraCheckboxes)
    {
        box->setEnabled(true);
    }
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
