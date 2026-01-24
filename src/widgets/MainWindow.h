#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <QLabel>
#include <QComboBox>
#include <QToolBar>
#include <QStatusBar>
#include <QMenu>
#include <QSplitter>
#include "CanvasView.h"
#include "io/ClassesLoader.h"
#include "io/AnnotationIO.h"

/**
 * @brief 主窗口
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // 文件操作
    void onOpenFolder();
    void onOpenImage();
    void onImportClasses();
    void onSaveAnnotation();

    // 导航
    void onFileSelected(QListWidgetItem* item);
    void onPrevImage();
    void onNextImage();

    // 标注操作
    void onAnnotationChanged();
    void onAnnotationSelected(int index);
    void onAnnotationDoubleClicked(int index);
    void onDeleteAnnotation();
    void onClassItemClicked(QListWidgetItem* item);

    // 工具
    void onModeChanged(QAction* action);
    void onZoomChanged(double scale);

    // 骨架配置
    void onEditSkeletonConfig();
    
    // 类别选择
    void onClassSelectRequested(int annotationIndex);
    void onClassListDoubleClicked(QListWidgetItem* item);
    void onContextMenuRequested(int annotationIndex, const QPoint& globalPos);

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
    bool m_autoSave = true;
    bool m_modified = false;
    bool m_classesLocked = false;  // 类别是否已锁定
    
    // 工具栏/菜单动作
    QAction* m_autoSaveAction = nullptr;

    void setupUI();
    void setupMenus();
    void setupToolBar();
    void setupConnections();

    void loadImage(int index);
    void saveCurrentAnnotation();
    void updateFileList();
    void updateAnnotationList();
    void updateClassList();
    void updateStatusBar();
    QString getAnnotationPath(const QString& imagePath);  // 获取标注文件路径
};
