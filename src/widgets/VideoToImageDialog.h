#ifndef VIDEOTOIMAGEDIALOG_H
#define VIDEOTOIMAGEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QTimer>
#include <QThread>
#include <opencv2/opencv.hpp>
#include <atomic>

class VideoExportWorker : public QObject {
    Q_OBJECT

public:
    explicit VideoExportWorker(QObject* parent = nullptr);

    void setParameters(const QString& videoPath, int startFrame, int endFrame,
                      int frameInterval, const QString& outputPath, const QString& format,
                      const QString& prefix, int digits);

public slots:
    void process();
    void requestStop();

signals:
    void progressUpdated(int current, int total, const QString& fileName);
    void finished(int totalExported);
    void errorOccurred(const QString& error);

private:
    QString m_videoPath;
    int m_startFrame;
    int m_endFrame;
    int m_frameInterval;
    QString m_outputPath;
    QString m_format;
    QString m_prefix;
    int m_digits;
    std::atomic<bool> m_stop;
};

class VideoToImageDialog : public QDialog {
    Q_OBJECT

public:
    explicit VideoToImageDialog(QWidget* parent = nullptr);
    ~VideoToImageDialog();

private:
    void setupUI();
    void loadVideo(const QString& path);
    void updatePreview(int frameNumber);
    void updateVideoInfo();
    void updateEstimatedCount();

private slots:
    void onBrowseVideo();
    void onBrowseOutput();
    void onFrameSliderChanged(int value);
    void onStartFrameChanged(int value);
    void onEndFrameChanged(int value);
    void onIntervalChanged(int value);
    void onExport();
    void onExportProgress(int current, int total, const QString& fileName);
    void onExportFinished(int totalExported);

private:
    // OpenCV 视频处理
    cv::VideoCapture m_capture;
    QString m_videoPath;
    int m_totalFrames;
    int m_fps;
    int m_width;
    int m_height;
    double m_duration;

    // UI 组件
    QLabel* m_previewLabel;
    QLabel* m_frameInfoLabel;
    QLabel* m_videoInfoLabel;
    QLineEdit* m_videoPathEdit;
    QPushButton* m_browseVideoBtn;
    QSlider* m_frameSlider;
    QSpinBox* m_startFrameSpinBox;
    QSpinBox* m_endFrameSpinBox;
    QSpinBox* m_frameIntervalSpinBox;
    QComboBox* m_formatCombo;
    QLineEdit* m_prefixEdit;
    QSpinBox* m_digitsSpinBox;
    QLineEdit* m_outputPathEdit;
    QPushButton* m_browseOutputBtn;
    QLabel* m_statusLabel;
    QPushButton* m_exportBtn;
    QPushButton* m_closeBtn;
    QTimer* m_previewTimer;

    // 后台线程
    QThread* m_exportThread;
    VideoExportWorker* m_exportWorker;
};

#endif // VIDEOTOIMAGEDIALOG_H
