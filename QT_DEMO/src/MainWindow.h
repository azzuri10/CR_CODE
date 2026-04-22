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
class QTextEdit;

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
    enum class TriggerMode {
        Single = 0,
        Continuous = 1
    };

    void setupUi();
    void refreshInputListByFile(const QString& filePath);
    void refreshInputListByFolder(const QString& folderPath);
    void showImage(const cv::Mat& img);
    bool processCurrentIndex(bool autoNext);
    bool processSingleHikFrame();
    InspectType currentInspectType() const;
    TriggerMode currentTriggerMode() const;
    bool isHikSource() const;
    void appendLog(const QString& text);

    DllInvoker invoker_;
    HikCamera hikCamera_;
    QStringList inputFiles_;
    int currentIndex_;
    int jobCounter_;
    bool running_;

    QLineEdit* editDllPath_;
    QLineEdit* editInputPath_;
    QComboBox* comboType_;
    QComboBox* comboSource_;
    QComboBox* comboTriggerMode_;
    QSpinBox* spinCameraId_;
    QSpinBox* spinHikIndex_;
    QSpinBox* spinIntervalMs_;
    QSpinBox* spinGrabTimeoutMs_;
    QLabel* imageLabel_;
    QLabel* statusLabel_;
    QTextEdit* logText_;
    QPushButton* btnContinuous_;
    QPushButton* btnStop_;
    QPushButton* btnHikSingle_;
    QTimer* runTimer_;
};
