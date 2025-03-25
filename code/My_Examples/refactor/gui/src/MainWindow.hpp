#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <QCheckBox>
#include "CameraSettingsWindow.hpp"
#include <QLineEdit>
#include "qcustomplot.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QPushButton *saveButton;
    bool savingEnabled = false;

private slots:
    void startStream();
    void stopStream();
    void updateFrame();

private:
    QLabel *videoLabel;
    QPushButton *startButton;
    QPushButton *stopButton;
    QTimer *timer;
    cv::VideoCapture cap;
    std::vector<QCheckBox *> cameraCheckboxes;
    CameraSettingsWindow *settingsWindow;
    QLineEdit *exposureEdit;
    QCustomPlot* plot;

};
