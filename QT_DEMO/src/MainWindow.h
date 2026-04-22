#pragma once

#include "DllInvoker.h"
#include "HikCamera.h"

#include <QMainWindow>
#include <QStringList>

class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private slots:
    void onBrowseDll();
    void onBrowseImage();
    void onBrowseFolder();
    void onLoadDll();
    void onStepRun();
    void onContinuousRun();
    void onStopRun();
    void onRunTick();
    void onHikCapture();

private:
    void setupUi();
    void refreshInputListByFile(const QString& filePath);
    void refreshInputListByFolder(const QString& folderPath);
    void showImage(const cv::Mat& img);
    bool processCurrentIndex(bool autoNext);
    InspectType currentInspectType() const;

    DllInvoker invoker_;
    HikCamera hikCamera_;
    QStringList inputFiles_;
    int currentIndex_;
    int jobCounter_;
    bool running_;

    QLineEdit* editDllPath_;
    QLineEdit* editInputPath_;
    QComboBox* comboType_;
    QSpinBox* spinCameraId_;
    QLabel* imageLabel_;
    QLabel* statusLabel_;
    QPushButton* btnContinuous_;
    QPushButton* btnStop_;
    QTimer* runTimer_;
};
