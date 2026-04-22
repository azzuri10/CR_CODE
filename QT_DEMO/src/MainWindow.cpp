#include "MainWindow.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QDir>

MainWindow::MainWindow() : currentIndex_(0), jobCounter_(0), running_(false) {
    setupUi();
}

void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    editDllPath_ = new QLineEdit(this);
    auto* btnBrowseDll = new QPushButton("Browse DLL", this);
    auto* btnLoadDll = new QPushButton("Load DLL", this);

    auto* dllRow = new QHBoxLayout();
    dllRow->addWidget(new QLabel("DLL Path:", this));
    dllRow->addWidget(editDllPath_);
    dllRow->addWidget(btnBrowseDll);
    dllRow->addWidget(btnLoadDll);

    editInputPath_ = new QLineEdit(this);
    auto* btnOpenImage = new QPushButton("Open Image", this);
    auto* btnOpenFolder = new QPushButton("Open Folder", this);
    auto* ioRow = new QHBoxLayout();
    ioRow->addWidget(new QLabel("Input Path:", this));
    ioRow->addWidget(editInputPath_);
    ioRow->addWidget(btnOpenImage);
    ioRow->addWidget(btnOpenFolder);

    comboType_ = new QComboBox(this);
    comboType_->addItem("Cap", static_cast<int>(InspectType::CapOmni));
    comboType_->addItem("Level", static_cast<int>(InspectType::Level));
    comboType_->addItem("Handle", static_cast<int>(InspectType::Handle));
    comboType_->addItem("Box Count", static_cast<int>(InspectType::Box));
    comboType_->addItem("Code", static_cast<int>(InspectType::Code));

    spinCameraId_ = new QSpinBox(this);
    spinCameraId_->setRange(0, 9);
    spinCameraId_->setValue(0);

    auto* btnStep = new QPushButton("Step Run", this);
    btnContinuous_ = new QPushButton("Continuous", this);
    btnStop_ = new QPushButton("Stop", this);
    btnStop_->setEnabled(false);

    auto* btnHik = new QPushButton("Hikvision Capture", this);
    auto* btnBasler = new QPushButton("Basler (Reserved)", this);

    auto* ctrlRow = new QHBoxLayout();
    ctrlRow->addWidget(new QLabel("Inspect Type:", this));
    ctrlRow->addWidget(comboType_);
    ctrlRow->addWidget(new QLabel("Camera ID:", this));
    ctrlRow->addWidget(spinCameraId_);
    ctrlRow->addWidget(btnStep);
    ctrlRow->addWidget(btnContinuous_);
    ctrlRow->addWidget(btnStop_);
    ctrlRow->addWidget(btnHik);
    ctrlRow->addWidget(btnBasler);

    imageLabel_ = new QLabel(this);
    imageLabel_->setMinimumSize(960, 600);
    imageLabel_->setStyleSheet("QLabel { background:#202020; color:#c0c0c0; }");
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setText("Inspection result preview");

    statusLabel_ = new QLabel("Status: DLL not loaded", this);

    root->addLayout(dllRow);
    root->addLayout(ioRow);
    root->addLayout(ctrlRow);
    root->addWidget(imageLabel_, 1);
    root->addWidget(statusLabel_);

    setCentralWidget(central);
    setWindowTitle("AOI Qt Test UI");
    resize(1280, 860);

    runTimer_ = new QTimer(this);
    runTimer_->setInterval(30);

    connect(btnBrowseDll, &QPushButton::clicked, this, &MainWindow::onBrowseDll);
    connect(btnLoadDll, &QPushButton::clicked, this, &MainWindow::onLoadDll);
    connect(btnOpenImage, &QPushButton::clicked, this, &MainWindow::onBrowseImage);
    connect(btnOpenFolder, &QPushButton::clicked, this, &MainWindow::onBrowseFolder);
    connect(btnStep, &QPushButton::clicked, this, &MainWindow::onStepRun);
    connect(btnContinuous_, &QPushButton::clicked, this, &MainWindow::onContinuousRun);
    connect(btnStop_, &QPushButton::clicked, this, &MainWindow::onStopRun);
    connect(runTimer_, &QTimer::timeout, this, &MainWindow::onRunTick);

    connect(btnHik, &QPushButton::clicked, this, &MainWindow::onHikCapture);
    connect(btnBasler, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, "Reserved", "Basler online capture is reserved for future SDK integration.");
    });
}

void MainWindow::onBrowseDll() {
    const QString path = QFileDialog::getOpenFileName(this, "Select DLL file", QString(), "DLL Files (*.dll)");
    if (!path.isEmpty()) editDllPath_->setText(path);
}

void MainWindow::onBrowseImage() {
    const QString file = QFileDialog::getOpenFileName(this, "Select image", QString(), "Image Files (*.png *.jpg *.jpeg *.bmp *.tif)");
    if (!file.isEmpty()) {
        editInputPath_->setText(file);
        refreshInputListByFile(file);
    }
}

void MainWindow::onBrowseFolder() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Select image folder");
    if (!dir.isEmpty()) {
        editInputPath_->setText(dir);
        refreshInputListByFolder(dir);
    }
}

void MainWindow::onLoadDll() {
    QString err;
    if (!invoker_.load(editDllPath_->text().trimmed(), &err)) {
        statusLabel_->setText("Status: DLL load failed - " + err);
        QMessageBox::warning(this, "Error", err);
        return;
    }
    statusLabel_->setText("Status: DLL loaded");
}

void MainWindow::onStepRun() {
    processCurrentIndex(true);
}

void MainWindow::onContinuousRun() {
    if (!invoker_.isLoaded() || inputFiles_.isEmpty()) {
        QMessageBox::information(this, "Info", "Please load DLL and select image/folder first.");
        return;
    }
    running_ = true;
    btnContinuous_->setEnabled(false);
    btnStop_->setEnabled(true);
    runTimer_->start();
}

void MainWindow::onStopRun() {
    running_ = false;
    runTimer_->stop();
    btnContinuous_->setEnabled(true);
    btnStop_->setEnabled(false);
    statusLabel_->setText("Status: stopped");
}

void MainWindow::onRunTick() {
    if (!running_) return;
    if (!processCurrentIndex(true)) {
        onStopRun();
    }
}

void MainWindow::onHikCapture() {
    if (!invoker_.isLoaded()) {
        QMessageBox::warning(this, "Error", "Please load DLL first");
        return;
    }

    std::string err;
    if (!hikCamera_.isOpened() && !hikCamera_.openFirst(&err)) {
        QMessageBox::warning(this, "Hikvision", QString::fromStdString(err));
        return;
    }

    cv::Mat frame;
    if (!hikCamera_.grab(&frame, &err, 1000)) {
        QMessageBox::warning(this, "Hikvision", QString::fromStdString(err));
        return;
    }

    cv::Mat out;
    QString inferErr;
    const int rv = invoker_.inspect(currentInspectType(), frame, jobCounter_++, spinCameraId_->value(), &out, &inferErr);
    if (out.empty()) out = frame;
    showImage(out);

    statusLabel_->setText(QString("Status: Hikvision frame inspected, RV=%1").arg(rv));
    if (!inferErr.isEmpty()) {
        statusLabel_->setText(statusLabel_->text() + " - " + inferErr);
    }
}

void MainWindow::refreshInputListByFile(const QString& filePath) {
    inputFiles_.clear();
    inputFiles_ << filePath;
    currentIndex_ = 0;
    statusLabel_->setText(QString("Status: loaded 1 image"));
}

void MainWindow::refreshInputListByFolder(const QString& folderPath) {
    QDir dir(folderPath);
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.tif";
    inputFiles_ = dir.entryList(filters, QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (QString& f : inputFiles_) f = dir.absoluteFilePath(f);
    currentIndex_ = 0;
    statusLabel_->setText(QString("Status: loaded %1 images").arg(inputFiles_.size()));
}

void MainWindow::showImage(const cv::Mat& img) {
    if (img.empty()) return;
    cv::Mat rgb;
    if (img.channels() == 1) cv::cvtColor(img, rgb, cv::COLOR_GRAY2RGB);
    else cv::cvtColor(img, rgb, cv::COLOR_BGR2RGB);

    QImage q(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
    imageLabel_->setPixmap(QPixmap::fromImage(q).scaled(imageLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

bool MainWindow::processCurrentIndex(bool autoNext) {
    if (!invoker_.isLoaded()) {
        QMessageBox::warning(this, "Error", "Please load DLL first");
        return false;
    }
    if (inputFiles_.isEmpty() || currentIndex_ >= inputFiles_.size()) {
        statusLabel_->setText("Status: no image to process");
        return false;
    }

    const QString file = inputFiles_[currentIndex_];
    cv::Mat src = cv::imread(file.toLocal8Bit().constData(), cv::IMREAD_COLOR);
    if (src.empty()) {
        statusLabel_->setText("Status: read failed - " + file);
        if (autoNext) ++currentIndex_;
        return currentIndex_ < inputFiles_.size();
    }

    cv::Mat out;
    QString err;
    const int rv = invoker_.inspect(currentInspectType(), src, jobCounter_++, spinCameraId_->value(), &out, &err);
    if (out.empty()) out = src;
    showImage(out);

    statusLabel_->setText(QString("Status: [%1/%2] %3, RV=%4")
        .arg(currentIndex_ + 1)
        .arg(inputFiles_.size())
        .arg(file)
        .arg(rv));
    if (!err.isEmpty()) statusLabel_->setText(statusLabel_->text() + " - " + err);

    if (autoNext) ++currentIndex_;
    return currentIndex_ < inputFiles_.size();
}

InspectType MainWindow::currentInspectType() const {
    return static_cast<InspectType>(comboType_->currentData().toInt());
}
