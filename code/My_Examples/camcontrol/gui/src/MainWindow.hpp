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

#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

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

/**
 * @class MainWindow
 * @brief Main application window for controlling camera streaming, monitoring offsets, and configuring settings.
 *
 * This window provides a GUI for managing connected cameras, starting/stopping streams,
 * viewing PTP offsets and delays, and opening settings dialogs.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a new MainWindow object.
     *
     * @param parent Optional parent widget.
     */
    MainWindow(QWidget *parent = nullptr);

    /**
     * @brief Destroys the MainWindow object.
     */
    ~MainWindow();

    /**
     * @brief Initializes the UI and internal structures.
     */
    void initialize();

private slots:
    /**
     * @brief Slot to start camera streaming.
     */
    void startStreaming();

    /**
     * @brief Slot to stop camera streaming.
     */
    void stopStreaming();

private:
    // UI setup helpers

    /**
     * @brief Sets up the top bar layout including control buttons and exposure edit.
     *
     * @param parentLayout The parent vertical layout.
     */
    void setupTopBar(QVBoxLayout *parentLayout);

    /**
     * @brief Sets up the streaming area and information display.
     *
     * @param parentLayout The parent horizontal layout.
     */
    void setupStreamAndInfoArea(QHBoxLayout *parentLayout);

    /**
     * @brief Sets up camera selection checkboxes.
     *
     * @param layout Layout to which checkboxes are added.
     */
    void setupCameraCheckboxes(QVBoxLayout *layout);

    /**
     * @brief Sets up the delay monitoring table.
     *
     * @param layout Layout to add the delay table into.
     */
    void setupDelayTable(QVBoxLayout *layout);

    /**
     * @brief Sets up the offset monitoring table.
     *
     * @param layout Layout to add the offset table into.
     */
    void setupOffsetTable(QVBoxLayout *layout);

    /**
     * @brief Connects UI elements to their respective slots.
     */
    void connectUI();

    /**
     * @brief Displays available camera checkboxes.
     *
     * @param layout Layout to add checkboxes to.
     */
    void displayCameraCheckboxes(QVBoxLayout *layout);

    /**
     * @brief Adds an "Open All" button to the UI.
     *
     * @param layout Layout to add the button into.
     */
    void addOpenAllButton(QVBoxLayout *layout);

    /**
     * @brief Sets up the UI structure for checkboxes.
     *
     * @param layout Layout to arrange the checkboxes.
     */
    void setupCheckboxUI(QVBoxLayout *layout);

    /**
     * @brief Plots offset values from a CSV file using QCustomPlot.
     *
     * @param layout Layout to insert the plot widget into.
     * @param csvFilePath Path to the CSV file.
     */
    void plotOffsetCSV(QVBoxLayout *layout, const QString &csvFilePath);

    /**
     * @brief Loads offset values from a CSV file into a table widget.
     *
     * @param table Target table widget.
     * @param label Label for additional information.
     * @param layout Parent layout for adjusting UI.
     */
    void loadOffsetTableFromCSV(QTableWidget *table, QLabel *label, QVBoxLayout *layout);

    /**
     * @brief Loads delay values from a CSV file into a delay table widget.
     *
     * @param delayTable Target table widget.
     * @param delayLabel Label for delay monitoring.
     */
    void loadDelayTableFromCSV(QTableWidget *delayTable, QLabel *delayLabel);

    /**
     * @brief Opens a dialog to schedule a delayed acquisition.
     */
    void openScheduleDialog();

private:
    // UI elements
    QPushButton *startButton;              ///< Button to start streaming.
    QPushButton *stopButton;               ///< Button to stop streaming.
    QPushButton *saveButton;               ///< Button to save measurements.
    QLineEdit *exposureEdit;               ///< Textbox to input exposure time.
    QVector<QCheckBox *> cameraCheckboxes; ///< Camera selection checkboxes.
    QTimer *timer;                         ///< Timer for updating stream or monitoring.
    QCustomPlot *plot;                     ///< Widget for plotting PTP offsets.
    QLabel *plotPlaceholder;               ///< Placeholder when no plot is available.
    QPushButton *settingsBtn;              ///< Button to open camera settings window.

    // Other components
    bool savingEnabled;                   ///< Flag indicating whether saving is active.
    bool isOpened;                        ///< Flag indicating if any camera is opened.
    CameraSettingsWindow *settingsWindow; ///< Pointer to the settings window.
    QVBoxLayout *plotLayout;              ///< Layout containing the plot widget.

    // Managers
    std::set<std::shared_ptr<rcg::Device>> availableCameras; ///< List of available cameras.
    std::list<std::shared_ptr<Camera>> openCamerasList;      ///< List of currently opened cameras.
    std::list<std::string> checkedCameraIDs;                 ///< IDs of checked/selected cameras.

    // Scheduling
    QPushButton *scheduleButton; ///< Button to schedule an acquisition.
    QTimer *scheduleTimer;       ///< Timer to trigger scheduled acquisitions.
    int scheduledDelayMs = 0;    ///< Scheduled delay in milliseconds.

    QLabel *compositeLabel = nullptr; ///< Label to display composite image or information.
};

#endif // MAINWINDOW_HPP
