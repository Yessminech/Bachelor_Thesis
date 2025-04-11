// PtpMonitorWorker.hpp
#pragma once
#include <QObject>
#include <list>
#include <memory>
#include "Camera.hpp"

class PtpMonitorWorker : public QObject {
    Q_OBJECT

public:
    PtpMonitorWorker(const std::list<std::shared_ptr<Camera>>& cameras, int timeout, bool debug);
    
public slots:
    void run();  // This replaces monitorPtpStatus()

signals:
    void finished(bool success);
    void statusUpdate(QString status);

private:
    std::list<std::shared_ptr<Camera>> openCamerasList;
    int monitorPtpStatusTimeoutMs;
    bool debug;
};
