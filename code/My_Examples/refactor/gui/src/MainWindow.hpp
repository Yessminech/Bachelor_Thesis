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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void initialize();

private slots:
    void startStreaming();
    void stopStreaming();

private:
    // UI setup helpers
    void setupTopBar(QVBoxLayout *parentLayout);
    void setupStreamAndInfoArea(QHBoxLayout *parentLayout);

    void setupCameraCheckboxes(QVBoxLayout *layout);
    void setupDelayTable(QVBoxLayout *layout);
    void setupOffsetTable(QVBoxLayout *layout);
    void setupOffsetPlot(QVBoxLayout *layout);
    void connectUI();
    void displayCameraCheckboxes(QVBoxLayout *layout);
    void addOpenAllButton(QVBoxLayout *layout);
    void setupCheckboxUI(QVBoxLayout *layout);
    void plotOffsetCSV(QVBoxLayout *layout, const QString &csvFilePath);
    void loadOffsetTableFromCSV(QTableWidget *table, QLabel *label, QVBoxLayout *layout);
    void loadDelayTableFromCSV(QTableWidget *delayTable, QLabel *delayLabel);

    // UI elements
    QPushButton *startButton;
    QPushButton *stopButton;
    QPushButton *saveButton;
    QLineEdit *exposureEdit;
    QVector<QCheckBox *> cameraCheckboxes;
    QTimer *timer;
    QCustomPlot *plot;
    QLabel *plotPlaceholder;
    QPushButton *settingsBtn; // Add this to class MainWindow

    // Other components
    bool savingEnabled;
    bool isOpened;
    CameraSettingsWindow *settingsWindow;
    QVBoxLayout *plotLayout;

    // Managers
    std::set<std::shared_ptr<rcg::Device>> availableCameras;
    std::list<std::shared_ptr<Camera>> openCamerasList;
    std::list<std::string> checkedCameraIDs;

    QPushButton *scheduleButton;
    QTimer *scheduleTimer;
    int scheduledDelayMs = 0;
    void openScheduleDialog();

    QLabel* compositeLabel = nullptr;

};