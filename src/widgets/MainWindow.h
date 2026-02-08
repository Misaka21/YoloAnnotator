#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <QLabel>
#include <QComboBox>
#include <QToolBar>
#include <QStatusBar>
#include <QMenu>
#include <QSplitter>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <atomic>
#include "CanvasView.h"
#include "io/ClassesLoader.h"
#include "io/AnnotationIO.h"
#include "io/DatasetConfig.h"

class YoloDetector;

// 后台检查标注状态的工作线程
class AnnotationStatusWorker : public QObject {
    Q_OBJECT
public:
    AnnotationStatusWorker() = default;
    void setFiles(const QStringList& files, const QString& folder, const QString& labelsFolder) {
        m_files = files;
        m_folder = folder;
        m_labelsFolder = labelsFolder;
    }
    void requestStop() { m_stop = true; }

public slots:
    void process();

signals:
    void fileStatusReady(int index, bool hasAnnotation);
    void progressUpdated(int current, int total);
    void finished();

private:
    QStringList m_files;
    QString m_folder;
    QString m_labelsFolder;
    std::atomic<bool> m_stop{false};
};

// 批量标注后台工作线程
class BatchAnnotateWorker : public QObject {
    Q_OBJECT
public:
    BatchAnnotateWorker(YoloDetector* detector, AnnotationIO* io)
        : m_detector(detector), m_annotationIO(io) {}

    void setFiles(const QStringList& files, const QString& folder,
                  const QString& labelsFolder, bool overwrite) {
        m_files = files;
        m_folder = folder;
        m_labelsFolder = labelsFolder;
        m_overwrite = overwrite;
    }
    void requestStop() { m_stop = true; }

public slots:
    void process();

signals:
    void progressUpdated(int index, int total, const QString& fileName, int detections);
    void fileCompleted(int fileIndex, const QString& fileName, int detections);
    void finished(int totalProcessed, int totalDetections);

private:
    YoloDetector* m_detector;
    AnnotationIO* m_annotationIO;
    QStringList m_files;
    QString m_folder;
    QString m_labelsFolder;
    bool m_overwrite = true;
    std::atomic<bool> m_stop{false};
};

/**
 * @brief 主窗口
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // 文件/项目操作
    void onOpenProject();
    void onCreateProjectFromTxt();
    void onOpenFolder();
    void onOpenImage();
    void onImportClasses();
    void onSaveAnnotation();
    void onDeleteImageFile();

    // 导航
    void onFileSelected(QListWidgetItem* item);
    void onPrevImage();
    void onNextImage();

    // 标注操作
    void onAnnotationChanged();
    void onAnnotationSelected(int index);
    void onAnnotationDoubleClicked(int index);
    void onDeleteAnnotation();
    void onClearAnnotations();
    void onClassItemClicked(QListWidgetItem* item);

    // 工具
    void onModeChanged(QAction* action);
    void onZoomChanged(double scale);
    void onImageLoadFailed(const QString& path);

    // 骨架配置
    void onEditSkeletonConfig();
    
    // 类别选择
    void onClassSelectRequested(int annotationIndex, QPoint globalPos);
    void onClassListDoubleClicked(QListWidgetItem* item);
    void onContextMenuRequested(int annotationIndex, const QPoint& globalPos);

    // 自动标注
    void onModelSettings();
    void onAutoAnnotateCurrent();
    void onAutoAnnotateBatch();
    void onAutoAnnotateUnannotated();
    void onStopBatchAnnotate();  // 停止批量标注
    void onBatchProgress(int index, int total, const QString& fileName, int detections);
    void onBatchFileCompleted(int fileIndex, const QString& fileName, int detections);
    void onBatchFinished(int totalProcessed, int totalDetections);

    // 数据集工具
    void onSplitDataset();
    void onDatasetAnalysis();
    void onBatchRename();

    // 视频转图片
    void onVideoToImage();

    // 后台标注状态检查
    void onFileStatusReady(int index, bool hasAnnotation);
    void onStatusCheckProgress(int current, int total);
    void onStatusCheckFinished();

private:
    // UI组件
    CanvasView* m_canvasView;
    QListWidget* m_fileList;
    QListWidget* m_annotationList;
    QListWidget* m_classList;
    QLabel* m_statusLabel;
    QLabel* m_zoomLabel;
    QComboBox* m_formatCombo;

    // 工具栏动作
    QAction* m_selectAction;
    QAction* m_bboxAction;
    QAction* m_keypointAction;

    // 数据
    QString m_currentFolder;
    QString m_labelsFolder;      // labels文件夹路径
    QStringList m_imageFiles;
    int m_currentIndex = -1;
    ClassesLoader m_classesLoader;
    AnnotationIO m_annotationIO;
    DatasetConfig m_datasetConfig;  // 项目配置
    bool m_autoSave = false;
    bool m_modified = false;
    bool m_classesLocked = false;  // 类别是否已锁定
    bool m_skipSaveReminder = false;  // 不再提醒未保存
    bool m_skipSaveAction = true;     // true=保存, false=放弃
    
    // 工具栏/菜单动作
    QAction* m_autoSaveAction = nullptr;
    QAction* m_autoAnnotateAction = nullptr;
    QAction* m_batchAnnotateAction = nullptr;

    // 自动标注工具栏
    QComboBox* m_annotateModeCombo = nullptr;  // 覆盖/追加模式
    QAction* m_quickAnnotateAction = nullptr;   // 快速标注按钮
    QAction* m_stopBatchAction = nullptr;       // 停止批量标注

    // 批量标注状态
    bool m_batchRunning = false;
    QThread* m_batchThread = nullptr;
    BatchAnnotateWorker* m_batchWorker = nullptr;

    // 后台状态检查
    QThread* m_statusThread = nullptr;
    AnnotationStatusWorker* m_statusWorker = nullptr;

    // 自动标注
    YoloDetector* m_detector = nullptr;

    void setupUI();
    void setupMenus();
    void setupToolBar();
    void setupConnections();

    void loadImage(int index);
    void saveCurrentAnnotation();
    bool checkUnsavedChanges();  // 检查未保存的更改，返回true表示可以继续
    void updateFileList();
    void startStatusCheck();     // 启动后台检查标注状态
    void stopStatusCheck();      // 停止后台检查
    void stopBatchAnnotate();    // 停止批量标注
    void updateAnnotationList();
    void updateClassList();
    void updateStatusBar();
    QString getAnnotationPath(const QString& imagePath);  // 获取标注文件路径

    // 项目加载辅助
    void loadProject(const DatasetConfig& config, const QString& knownImagePath = QString());
    void loadImagesFromPath(const QString& imagePath);
    QString findYamlInFolder(const QString& folder);
    QString findClassesTxtInFolder(const QString& folder);
};
