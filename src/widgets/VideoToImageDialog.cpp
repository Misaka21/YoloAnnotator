#include "VideoToImageDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// ========== VideoExportWorker Implementation ==========

VideoExportWorker::VideoExportWorker(QObject* parent)
    : QObject(parent), m_stop(false) {}

void VideoExportWorker::setParameters(const QString& videoPath, int startFrame, int endFrame,
                                      int frameInterval, const QString& outputPath, const QString& format,
                                      const QString& prefix, int digits) {
    m_videoPath = videoPath;
    m_startFrame = startFrame;
    m_endFrame = endFrame;
    m_frameInterval = frameInterval;
    m_outputPath = outputPath;
    m_format = format;
    m_prefix = prefix;
    m_digits = digits;
    m_stop = false;
}

void VideoExportWorker::requestStop() {
    m_stop = true;
}

void VideoExportWorker::process() {
    cv::VideoCapture cap;
    bool opened = false;

    std::string videoPath = m_videoPath.toStdString();

    // 尝试多种后端打开视频
#ifdef _WIN32
    // Windows: 优先尝试MSMF后端
    opened = cap.open(videoPath, cv::CAP_MSMF);
#else
    // Linux/macOS: 使用默认后端
    opened = cap.open(videoPath);
#endif

    // 尝试FFMPEG后端
    if (!opened) {
        opened = cap.open(videoPath, cv::CAP_FFMPEG);
    }

    // 尝试ANY后端
    if (!opened) {
        opened = cap.open(videoPath, cv::CAP_ANY);
    }

    if (!opened) {
        emit errorOccurred("无法打开视频文件");
        emit finished(0);
        return;
    }

    int totalToExtract = (m_endFrame - m_startFrame) / m_frameInterval + 1;
    int extracted = 0;
    int seqNumber = 1;  // sequential numbering starting from 1

    for (int i = m_startFrame; i <= m_endFrame && !m_stop; i += m_frameInterval) {
        // 跳转到帧
        cap.set(cv::CAP_PROP_POS_FRAMES, i);

        // 读取
        cv::Mat frame;
        if (!cap.read(frame)) {
            continue;
        }

        // 生成文件名 {prefix}_{seq}.{format}
        QString prefix = m_prefix.isEmpty() ? "frame" : m_prefix;
        int digits = qBound(3, m_digits, 8);
        QString fileName = QString("%1_%2.%3")
            .arg(prefix)
            .arg(seqNumber++, digits, 10, QChar('0'))
            .arg(m_format);
        QString filePath = m_outputPath + "/" + fileName;

        // 保存（JPEG质量95）
        std::vector<int> params;
        if (m_format == "jpg") {
            params = {cv::IMWRITE_JPEG_QUALITY, 95};
        }

        if (cv::imwrite(filePath.toStdString(), frame, params)) {
            extracted++;
            emit progressUpdated(extracted, totalToExtract, fileName);
        }
    }

    cap.release();
    emit finished(extracted);
}

// ========== VideoToImageDialog Implementation ==========

VideoToImageDialog::VideoToImageDialog(QWidget* parent)
    : QDialog(parent),
      m_totalFrames(0),
      m_fps(0),
      m_width(0),
      m_height(0),
      m_duration(0.0),
      m_exportThread(nullptr),
      m_exportWorker(nullptr) {
    setupUI();
    setWindowTitle("视频转图片");
    resize(600, 800);
}

VideoToImageDialog::~VideoToImageDialog() {
    if (m_capture.isOpened()) {
        m_capture.release();
    }
    if (m_exportThread && m_exportThread->isRunning()) {
        if (m_exportWorker) {
            m_exportWorker->requestStop();
        }
        m_exportThread->quit();
        m_exportThread->wait();
    }
}

void VideoToImageDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ========== 视频预览组 ==========
    QGroupBox* previewGroup = new QGroupBox("视频预览", this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setFixedHeight(400);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("QLabel { background-color: #2b2b2b; }");
    m_previewLabel->setText("未加载视频");
    previewLayout->addWidget(m_previewLabel);

    QHBoxLayout* infoLayout = new QHBoxLayout();
    m_frameInfoLabel = new QLabel("帧 0/0", this);
    m_videoInfoLabel = new QLabel("", this);
    m_videoInfoLabel->setAlignment(Qt::AlignRight);
    infoLayout->addWidget(m_frameInfoLabel);
    infoLayout->addWidget(m_videoInfoLabel);
    previewLayout->addLayout(infoLayout);

    mainLayout->addWidget(previewGroup);

    // ========== 视频文件组 ==========
    QGroupBox* fileGroup = new QGroupBox("视频文件", this);
    QHBoxLayout* fileLayout = new QHBoxLayout(fileGroup);

    m_videoPathEdit = new QLineEdit(this);
    m_videoPathEdit->setPlaceholderText("选择视频文件...");
    m_videoPathEdit->setReadOnly(true);
    m_browseVideoBtn = new QPushButton("浏览...", this);
    fileLayout->addWidget(m_videoPathEdit);
    fileLayout->addWidget(m_browseVideoBtn);

    mainLayout->addWidget(fileGroup);

    // ========== 帧选择组 ==========
    QGroupBox* frameGroup = new QGroupBox("帧选择", this);
    QVBoxLayout* frameLayout = new QVBoxLayout(frameGroup);

    // 预览滑块
    QHBoxLayout* sliderLayout = new QHBoxLayout();
    sliderLayout->addWidget(new QLabel("预览:", this));
    m_frameSlider = new QSlider(Qt::Horizontal, this);
    m_frameSlider->setRange(0, 0);
    m_frameSlider->setEnabled(false);
    sliderLayout->addWidget(m_frameSlider, 1);
    frameLayout->addLayout(sliderLayout);

    // 帧范围和间隔
    QFormLayout* frameFormLayout = new QFormLayout();

    m_startFrameSpinBox = new QSpinBox(this);
    m_startFrameSpinBox->setRange(0, 0);
    m_startFrameSpinBox->setEnabled(false);
    frameFormLayout->addRow("起始帧:", m_startFrameSpinBox);

    m_endFrameSpinBox = new QSpinBox(this);
    m_endFrameSpinBox->setRange(0, 0);
    m_endFrameSpinBox->setEnabled(false);
    frameFormLayout->addRow("结束帧:", m_endFrameSpinBox);

    QHBoxLayout* intervalLayout = new QHBoxLayout();
    m_frameIntervalSpinBox = new QSpinBox(this);
    m_frameIntervalSpinBox->setRange(1, 1000);
    m_frameIntervalSpinBox->setValue(1);
    m_frameIntervalSpinBox->setEnabled(false);
    intervalLayout->addWidget(m_frameIntervalSpinBox);
    intervalLayout->addWidget(new QLabel("(每N帧提取一张)", this));
    intervalLayout->addStretch();
    frameFormLayout->addRow("帧间隔:", intervalLayout);

    frameLayout->addLayout(frameFormLayout);
    mainLayout->addWidget(frameGroup);

    // ========== 输出设置组 ==========
    QGroupBox* outputGroup = new QGroupBox("输出设置", this);
    QFormLayout* outputFormLayout = new QFormLayout(outputGroup);

    QHBoxLayout* outputPathLayout = new QHBoxLayout();
    m_outputPathEdit = new QLineEdit(this);
    m_outputPathEdit->setPlaceholderText("选择输出目录...");
    m_browseOutputBtn = new QPushButton("浏览...", this);
    m_browseOutputBtn->setEnabled(false);
    outputPathLayout->addWidget(m_outputPathEdit);
    outputPathLayout->addWidget(m_browseOutputBtn);
    outputFormLayout->addRow("输出路径:", outputPathLayout);

    m_prefixEdit = new QLineEdit("frame", this);
    m_prefixEdit->setPlaceholderText("文件名前缀，例如：frame, img");
    m_prefixEdit->setEnabled(false);
    outputFormLayout->addRow("文件名前缀:", m_prefixEdit);

    m_digitsSpinBox = new QSpinBox(this);
    m_digitsSpinBox->setRange(3, 8);
    m_digitsSpinBox->setValue(6);
    m_digitsSpinBox->setEnabled(false);
    outputFormLayout->addRow("序号位数:", m_digitsSpinBox);

    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem("JPEG (*.jpg)");
    m_formatCombo->addItem("PNG (*.png)");
    m_formatCombo->setEnabled(false);
    outputFormLayout->addRow("图片格式:", m_formatCombo);

    mainLayout->addWidget(outputGroup);

    // ========== 底部按钮 ==========
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("就绪", this);
    m_exportBtn = new QPushButton("导出", this);
    m_exportBtn->setEnabled(false);
    m_closeBtn = new QPushButton("关闭", this);

    buttonLayout->addWidget(m_statusLabel);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_exportBtn);
    buttonLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(buttonLayout);

    // ========== 节流定时器 ==========
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(100);

    // ========== 信号连接 ==========
    connect(m_browseVideoBtn, &QPushButton::clicked, this, &VideoToImageDialog::onBrowseVideo);
    connect(m_browseOutputBtn, &QPushButton::clicked, this, &VideoToImageDialog::onBrowseOutput);
    connect(m_frameSlider, &QSlider::valueChanged, this, &VideoToImageDialog::onFrameSliderChanged);
    connect(m_startFrameSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &VideoToImageDialog::onStartFrameChanged);
    connect(m_endFrameSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &VideoToImageDialog::onEndFrameChanged);
    connect(m_frameIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &VideoToImageDialog::onIntervalChanged);
    connect(m_exportBtn, &QPushButton::clicked, this, &VideoToImageDialog::onExport);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_previewTimer, &QTimer::timeout, [this]() {
        updatePreview(m_frameSlider->value());
    });
}

void VideoToImageDialog::loadVideo(const QString& path) {
    // 释放旧capture
    if (m_capture.isOpened()) {
        m_capture.release();
    }

    // 打开视频 - 尝试多种后端
    bool opened = false;
    std::string videoPath = path.toStdString();

#ifdef _WIN32
    // Windows: 优先尝试MSMF后端
    opened = m_capture.open(videoPath, cv::CAP_MSMF);
#else
    // Linux/macOS: 使用默认后端
    opened = m_capture.open(videoPath);
#endif

    // 尝试FFMPEG后端
    if (!opened) {
        opened = m_capture.open(videoPath, cv::CAP_FFMPEG);
    }

    // 尝试ANY后端
    if (!opened) {
        opened = m_capture.open(videoPath, cv::CAP_ANY);
    }

    if (!opened) {
        QString errorMsg = QString(
            "无法打开视频文件。\n\n"
            "可能的原因：\n"
            "1. 文件路径包含中文或特殊字符\n"
            "2. 视频编码格式不支持\n"
            "3. OpenCV视频解码器未正确安装\n\n"
            "建议：\n"
            "- 将视频移到纯英文路径（如 C:\\test\\video.mp4）\n"
            "- 尝试转换视频格式（推荐H.264编码的MP4）\n\n"
            "文件: %1"
        ).arg(path);
        QMessageBox::critical(this, "错误", errorMsg);
        return;
    }

    // 读取视频信息
    m_videoPath = path;
    m_totalFrames = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_COUNT));
    m_fps = static_cast<int>(m_capture.get(cv::CAP_PROP_FPS));
    m_width = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_WIDTH));
    m_height = static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    m_duration = m_totalFrames / static_cast<double>(m_fps);

    // 更新UI控件范围
    m_frameSlider->setRange(0, m_totalFrames - 1);
    m_frameSlider->setValue(0);
    m_frameSlider->setEnabled(true);

    m_startFrameSpinBox->setRange(0, m_totalFrames - 1);
    m_startFrameSpinBox->setValue(0);
    m_startFrameSpinBox->setEnabled(true);

    m_endFrameSpinBox->setRange(0, m_totalFrames - 1);
    m_endFrameSpinBox->setValue(m_totalFrames - 1);
    m_endFrameSpinBox->setEnabled(true);

    m_frameIntervalSpinBox->setEnabled(true);
    m_prefixEdit->setEnabled(true);
    m_digitsSpinBox->setEnabled(true);
    m_formatCombo->setEnabled(true);
    m_browseOutputBtn->setEnabled(true);
    m_exportBtn->setEnabled(true);

    // 设置默认输出路径
    QFileInfo fileInfo(path);
    QString defaultOutputPath = fileInfo.absolutePath() + "/frames";
    m_outputPathEdit->setText(defaultOutputPath);

    // 显示第0帧
    updatePreview(0);
    updateVideoInfo();
    updateEstimatedCount();
}

void VideoToImageDialog::updatePreview(int frameNumber) {
    if (!m_capture.isOpened() || frameNumber < 0 || frameNumber >= m_totalFrames) {
        return;
    }

    // 跳转到指定帧
    m_capture.set(cv::CAP_PROP_POS_FRAMES, frameNumber);

    // 读取帧
    cv::Mat frame;
    if (!m_capture.read(frame)) {
        return;
    }

    // BGR -> RGB 转换
    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

    // OpenCV Mat -> QImage
    QImage img(frame.data, frame.cols, frame.rows,
               static_cast<int>(frame.step), QImage::Format_RGB888);
    QImage copy = img.copy(); // 深拷贝避免数据失效

    // 缩放并显示
    QPixmap pixmap = QPixmap::fromImage(copy);
    QSize previewSize = m_previewLabel->size();
    pixmap = pixmap.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_previewLabel->setPixmap(pixmap);
}

void VideoToImageDialog::updateVideoInfo() {
    QString info = QString("%1x%2 | %3fps | %4s")
        .arg(m_width).arg(m_height)
        .arg(m_fps)
        .arg(m_duration, 0, 'f', 1);
    m_videoInfoLabel->setText(info);
}

void VideoToImageDialog::updateEstimatedCount() {
    if (m_totalFrames == 0) {
        return;
    }

    int startFrame = m_startFrameSpinBox->value();
    int endFrame = m_endFrameSpinBox->value();
    int interval = m_frameIntervalSpinBox->value();

    int estimatedCount = (endFrame - startFrame) / interval + 1;
    m_statusLabel->setText(QString("将导出约 %1 张图片").arg(estimatedCount));
}

void VideoToImageDialog::onBrowseVideo() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择视频文件",
        QString(),
        "视频文件 (*.mp4 *.avi *.mov *.mkv *.flv *.wmv);;所有文件 (*.*)"
    );

    if (!fileName.isEmpty()) {
        m_videoPathEdit->setText(fileName);

        // 显示OpenCV构建信息（仅在第一次打开时）
        static bool firstTime = true;
        if (firstTime) {
            firstTime = false;
            QString buildInfo = QString("OpenCV版本: %1.%2.%3")
                .arg(CV_VERSION_MAJOR)
                .arg(CV_VERSION_MINOR)
                .arg(CV_VERSION_REVISION);
            qDebug() << buildInfo;
            qDebug() << "视频后端:" << cv::getBuildInformation().c_str();
        }

        loadVideo(fileName);
    }
}

void VideoToImageDialog::onBrowseOutput() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "选择输出目录",
        m_outputPathEdit->text()
    );

    if (!dir.isEmpty()) {
        m_outputPathEdit->setText(dir);
    }
}

void VideoToImageDialog::onFrameSliderChanged(int value) {
    m_frameInfoLabel->setText(QString("帧 %1/%2").arg(value).arg(m_totalFrames));
    m_previewTimer->start(); // 重启定时器
}

void VideoToImageDialog::onStartFrameChanged(int value) {
    // 联动：起始帧 <= 结束帧
    if (value > m_endFrameSpinBox->value()) {
        m_endFrameSpinBox->setValue(value);
    }
    updateEstimatedCount();
}

void VideoToImageDialog::onEndFrameChanged(int value) {
    // 联动：结束帧 >= 起始帧
    if (value < m_startFrameSpinBox->value()) {
        m_startFrameSpinBox->setValue(value);
    }
    updateEstimatedCount();
}

void VideoToImageDialog::onIntervalChanged(int) {
    updateEstimatedCount();
}

void VideoToImageDialog::onExport() {
    // 验证输入
    if (m_videoPath.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先选择视频文件");
        return;
    }

    if (m_outputPathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择输出路径");
        return;
    }

    // 创建输出目录
    QString outputPath = m_outputPathEdit->text();
    QDir().mkpath(outputPath);

    // 确认对话框
    int totalFrames = (m_endFrameSpinBox->value() - m_startFrameSpinBox->value())
                      / m_frameIntervalSpinBox->value() + 1;
    auto reply = QMessageBox::question(this, "确认导出",
        QString("将提取 %1 张图片\n输出路径: %2\n\n是否继续?")
            .arg(totalFrames).arg(outputPath));
    if (reply != QMessageBox::Yes) {
        return;
    }

    // 创建线程和Worker
    m_exportThread = new QThread(this);
    m_exportWorker = new VideoExportWorker();
    m_exportWorker->setParameters(
        m_videoPath,
        m_startFrameSpinBox->value(),
        m_endFrameSpinBox->value(),
        m_frameIntervalSpinBox->value(),
        outputPath,
        m_formatCombo->currentIndex() == 0 ? "jpg" : "png",
        m_prefixEdit->text(),
        m_digitsSpinBox->value()
    );
    m_exportWorker->moveToThread(m_exportThread);

    // 连接信号（自动清理）
    connect(m_exportThread, &QThread::started, m_exportWorker, &VideoExportWorker::process);
    connect(m_exportWorker, &VideoExportWorker::progressUpdated,
            this, &VideoToImageDialog::onExportProgress);
    connect(m_exportWorker, &VideoExportWorker::finished,
            this, &VideoToImageDialog::onExportFinished);
    connect(m_exportWorker, &VideoExportWorker::finished, m_exportThread, &QThread::quit);
    connect(m_exportThread, &QThread::finished, m_exportWorker, &QObject::deleteLater);
    connect(m_exportThread, &QThread::finished, m_exportThread, &QObject::deleteLater);

    // 禁用导出按钮，启动线程
    m_exportBtn->setEnabled(false);
    m_statusLabel->setText("正在导出...");
    m_exportThread->start();
}

void VideoToImageDialog::onExportProgress(int current, int total, const QString& fileName) {
    int percent = current * 100 / total;
    m_statusLabel->setText(QString("正在导出: %1 (%2/%3) - %4%")
        .arg(fileName).arg(current).arg(total).arg(percent));
}

void VideoToImageDialog::onExportFinished(int totalExported) {
    m_exportBtn->setEnabled(true);
    m_statusLabel->setText("就绪");
    QMessageBox::information(this, "导出完成",
        QString("成功导出 %1 张图片到:\n%2")
            .arg(totalExported).arg(m_outputPathEdit->text()));
    updateEstimatedCount();
}
