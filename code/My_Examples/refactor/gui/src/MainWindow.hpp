#pragma once
#include "DeviceManager.hpp"
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
    void initialize();

private slots:
    void startStreaming();
    void stopStreaming();
    void updateFrame();

private:
    // UI setup helpers
    void setupTopBar(QVBoxLayout* parentLayout);
    void setupStreamAndInfoArea(QHBoxLayout* parentLayout);
    void setupVideoFeed(QVBoxLayout* layout);
    void setupCameraCheckboxes(QVBoxLayout* layout);
    void setupDelayTable(QVBoxLayout* layout);
    void setupOffsetTable(QVBoxLayout* layout);
    void setupOffsetPlot(QVBoxLayout* layout);
    void connectUI();
    void displayCameraCheckboxes(QVBoxLayout *layout);
    void addOpenAllButton(QVBoxLayout *layout);
    void setupCheckboxUI(QVBoxLayout *layout);
    void plotOffsetCSV(QVBoxLayout *layout, const QString &csvFilePath);
    void loadOffsetTableFromCSV(QTableWidget* table, QLabel* label, QVBoxLayout* layout);

    // UI elements
    QPushButton* startButton;
    QPushButton* stopButton;
    QPushButton* saveButton;
    QLineEdit* exposureEdit;
    QLabel* videoLabel;
    QVector<QCheckBox*> cameraCheckboxes;
    QTimer* timer;
    QCustomPlot* plot;
    QLabel* plotPlaceholder;

    // Other components
    cv::VideoCapture cap;
    bool savingEnabled;
    bool isOpened;
    CameraSettingsWindow* settingsWindow;
    QVBoxLayout* plotLayout;

    // Managers
    std::set<std::shared_ptr<rcg::Device>> availableCameras;
    std::list<std::shared_ptr<Camera>> openCamerasList;
    std::list<std::string> checkedCameraIDs; 

};