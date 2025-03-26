#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <QVector>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include "qcustomplot.h"
#include <opencv2/opencv.hpp>

class CameraSettingsWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void startStream();
    void stopStream();
    void updateFrame();

private:
    // UI setup helpers
    void setupTopBar(QVBoxLayout* parentLayout);
    void setupStreamAndInfoArea(QVBoxLayout* parentLayout);
    void setupVideoFeed(QVBoxLayout* layout);
    void setupCameraCheckboxes(QVBoxLayout* layout);
    void setupControls(QVBoxLayout* layout);
    void setupDelayTable(QVBoxLayout* layout);
    void setupOffsetTable(QVBoxLayout* layout);
    void setupOffsetPlot(QVBoxLayout* layout);
    void connectUI();

    // UI elements
    QPushButton* startButton;
    QPushButton* stopButton;
    QPushButton* saveButton;
    QLineEdit* exposureEdit;
    QLabel* videoLabel;
    QVector<QCheckBox*> cameraCheckboxes;
    QTimer* timer;
    QCustomPlot* plot;

    // Other components
    cv::VideoCapture cap;
    bool savingEnabled;
    CameraSettingsWindow* settingsWindow;
};