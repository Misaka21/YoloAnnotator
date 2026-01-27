#include "MainWindow.h"
#include "NewProjectDialog.h"
#include "SplitDatasetDialog.h"
#include "SkeletonConfigDialog.h"
#include "DatasetAnalysisDialog.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QShortcut>
#include <QInputDialog>
#include <QActionGroup>
#include <QProgressDialog>
#include <QApplication>
#include <QColorDialog>
#include <QDialog>
#include <QCheckBox>
#include <QPushButton>
#include "inference/YoloDetector.h"
#include "inference/ModelSettingsDialog.h"

// BatchAnnotateWorker 实现
void BatchAnnotateWorker::process() {
    int total = m_files.size();
    int totalDetections = 0;

    for (int i = 0; i < total; ++i) {
        if (m_stop) break;

        QString fileName = m_files[i];
        QString imagePath = m_folder + "/" + fileName;

        emit progressUpdated(i, total, fileName, 0);

        QImage image(imagePath);
        if (image.isNull()) continue;

        auto annotations = m_detector->detect(image);
        int detCount = annotations.size();
        totalDetections += detCount;

        // 计算标签路径
        QString txtPath;
        if (!m_labelsFolder.isEmpty()) {
            txtPath = m_labelsFolder + "/" + QFileInfo(fileName).completeBaseName() + ".txt";
        } else {
            txtPath = m_folder + "/" + QFileInfo(fileName).completeBaseName() + ".txt";
        }

        // 根据模式保存
        if (m_overwrite) {
            // 即使没有检测结果，也创建空文件标记为"已处理"
            if (annotations.isEmpty()) {
                QFile file(txtPath);
                file.open(QIODevice::WriteOnly | QIODevice::Text);
                file.close();
            } else {
                m_annotationIO->save(txtPath, annotations);
            }
        } else {
            QVector<Annotation> existing;
            if (QFile::exists(txtPath)) {
                existing = m_annotationIO->load(txtPath);
            }
            existing.append(annotations);
            // 追加模式下也需要处理空结果
            if (existing.isEmpty()) {
                QFile file(txtPath);
                file.open(QIODevice::WriteOnly | QIODevice::Text);
                file.close();
            } else {
                m_annotationIO->save(txtPath, existing);
            }
        }

        emit fileCompleted(i, fileName, detCount);
    }

    emit finished(m_stop ? -1 : total, totalDetections);
}

// AnnotationStatusWorker 实现
void AnnotationStatusWorker::process() {
    for (int i = 0; i < m_files.size(); ++i) {
        if (m_stop) break;

        QString fileName = m_files[i];
        QString txtPath;
        if (!m_labelsFolder.isEmpty()) {
            txtPath = m_labelsFolder + "/" + QFileInfo(fileName).completeBaseName() + ".txt";
        } else {
            txtPath = m_folder + "/" + QFileInfo(fileName).completeBaseName() + ".txt";
        }

        bool hasAnnotation = QFile::exists(txtPath);
        emit fileStatusReady(i, hasAnnotation);
    }

    emit finished();
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_detector = new YoloDetector(this);
    setupUI(); setupMenus(); setupToolBar(); setupConnections();
    setWindowTitle("YOLO 标注工具"); resize(1400, 900);
}

MainWindow::~MainWindow() {
    stopStatusCheck();
    if (m_modified && m_autoSave) saveCurrentAnnotation();
}

void MainWindow::setupUI() {
    QWidget* central = new QWidget(this); setCentralWidget(central);
    QHBoxLayout* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(5,5,5,5); mainLayout->setSpacing(5);
    QGroupBox* fileGroup = new QGroupBox("文件列表", this);
    QVBoxLayout* fileLayout = new QVBoxLayout(fileGroup);
    m_fileList = new QListWidget(this); fileLayout->addWidget(m_fileList);
    fileGroup->setFixedWidth(200);
    m_canvasView = new CanvasView(this);
    QWidget* rightPanel = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0,0,0,0); rightPanel->setFixedWidth(220);
    QGroupBox* classGroup = new QGroupBox("类别", this);
    QVBoxLayout* classLayout = new QVBoxLayout(classGroup);
    m_classList = new QListWidget(this); classLayout->addWidget(m_classList);
    classGroup->setMaximumHeight(200);
    QGroupBox* annGroup = new QGroupBox("标注", this);
    QVBoxLayout* annLayout = new QVBoxLayout(annGroup);
    m_annotationList = new QListWidget(this); annLayout->addWidget(m_annotationList);
    QGroupBox* formatGroup = new QGroupBox("格式", this);
    QVBoxLayout* formatLayout = new QVBoxLayout(formatGroup);
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem("目标检测 (Detection)");
    m_formatCombo->addItem("姿态估计 (Pose)");
    formatLayout->addWidget(m_formatCombo); formatGroup->setMaximumHeight(80);
    rightLayout->addWidget(classGroup); rightLayout->addWidget(annGroup);
    rightLayout->addWidget(formatGroup);
    mainLayout->addWidget(fileGroup); mainLayout->addWidget(m_canvasView, 1);
    mainLayout->addWidget(rightPanel);
    m_statusLabel = new QLabel("就绪", this); m_zoomLabel = new QLabel("100%", this);
    statusBar()->addWidget(m_statusLabel, 1); statusBar()->addPermanentWidget(m_zoomLabel);
}

void MainWindow::setupMenus() {
    QMenu* fileMenu = menuBar()->addMenu("文件(&F)");

    // 项目操作
    QAction* openProjectAction = fileMenu->addAction("打开项目(&P)...");
    openProjectAction->setShortcut(QKeySequence("Ctrl+Shift+O"));
    connect(openProjectAction, &QAction::triggered, this, &MainWindow::onOpenProject);

    QAction* createProjectAction = fileMenu->addAction("从 classes.txt 创建项目...");
    connect(createProjectAction, &QAction::triggered, this, &MainWindow::onCreateProjectFromTxt);

    fileMenu->addSeparator();

    QAction* openFolderAction = fileMenu->addAction("打开文件夹(&O)...");
    openFolderAction->setShortcut(QKeySequence("Ctrl+O"));
    connect(openFolderAction, &QAction::triggered, this, &MainWindow::onOpenFolder);
    QAction* importClassesAction = fileMenu->addAction("导入类别(&C)...");
    connect(importClassesAction, &QAction::triggered, this, &MainWindow::onImportClasses);
    fileMenu->addSeparator();
    QAction* saveAction = fileMenu->addAction("保存(&S)");
    saveAction->setShortcut(QKeySequence("Ctrl+S"));
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveAnnotation);
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction("退出(&X)");
    exitAction->setShortcut(QKeySequence("Alt+F4"));
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
    QMenu* editMenu = menuBar()->addMenu("编辑(&E)");
    QAction* undoAction = editMenu->addAction("撤销(&U)");
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, m_canvasView, &CanvasView::undo);
    QAction* redoAction = editMenu->addAction("重做(&R)");
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, m_canvasView, &CanvasView::redo);
    editMenu->addSeparator();
    QAction* deleteAction = editMenu->addAction("删除标注(&D)");
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::onDeleteAnnotation);
    QMenu* viewMenu = menuBar()->addMenu("视图(&V)");
    QAction* zoomInAction = viewMenu->addAction("放大(&I)");
    zoomInAction->setShortcut(QKeySequence("Ctrl++"));
    connect(zoomInAction, &QAction::triggered, m_canvasView, &CanvasView::zoomIn);
    QAction* zoomOutAction = viewMenu->addAction("缩小(&O)");
    zoomOutAction->setShortcut(QKeySequence("Ctrl+-"));
    connect(zoomOutAction, &QAction::triggered, m_canvasView, &CanvasView::zoomOut);
    QAction* zoomFitAction = viewMenu->addAction("适应窗口(&F)");
    zoomFitAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(zoomFitAction, &QAction::triggered, m_canvasView, &CanvasView::zoomFit);
    viewMenu->addSeparator();

    // 十字线选项
    QAction* crossHairAction = viewMenu->addAction("显示十字辅助线(&H)");
    crossHairAction->setCheckable(true);
    crossHairAction->setChecked(m_canvasView->crossHairEnabled());
    connect(crossHairAction, &QAction::toggled, m_canvasView, &CanvasView::setCrossHairEnabled);

    QAction* crossHairColorAction = viewMenu->addAction("十字线颜色...");
    connect(crossHairColorAction, &QAction::triggered, [this]() {
        QColor color = QColorDialog::getColor(m_canvasView->crossHairColor(), this, "选择十字线颜色", QColorDialog::ShowAlphaChannel);
        if (color.isValid()) {
            m_canvasView->setCrossHairColor(color);
        }
    });
    QMenu* toolsMenu = menuBar()->addMenu("工具(&T)");
    QAction* skeletonAction = toolsMenu->addAction("配置骨架(&S)...");
    connect(skeletonAction, &QAction::triggered, this, &MainWindow::onEditSkeletonConfig);

    toolsMenu->addSeparator();
    QAction* splitAction = toolsMenu->addAction("分割数据集(&D)...");
    connect(splitAction, &QAction::triggered, this, &MainWindow::onSplitDataset);

    QAction* analysisAction = toolsMenu->addAction("数据集分析(&A)...");
    connect(analysisAction, &QAction::triggered, this, &MainWindow::onDatasetAnalysis);

    // 自动标注菜单
    QMenu* autoMenu = menuBar()->addMenu("自动标注(&A)");
    QAction* modelSettingsAction = autoMenu->addAction("模型设置(&S)...");
    connect(modelSettingsAction, &QAction::triggered, this, &MainWindow::onModelSettings);
    autoMenu->addSeparator();
    m_autoAnnotateAction = autoMenu->addAction("标注当前图片(&C)");
    m_autoAnnotateAction->setShortcut(QKeySequence("Ctrl+G"));
    m_autoAnnotateAction->setEnabled(false);
    connect(m_autoAnnotateAction, &QAction::triggered, this, &MainWindow::onAutoAnnotateCurrent);
    m_batchAnnotateAction = autoMenu->addAction("批量标注所有(&B)...");
    m_batchAnnotateAction->setEnabled(false);
    connect(m_batchAnnotateAction, &QAction::triggered, this, &MainWindow::onAutoAnnotateBatch);
    QAction* unannotatedAction = autoMenu->addAction("标注未标注图片(&U)...");
    unannotatedAction->setEnabled(false);
    connect(unannotatedAction, &QAction::triggered, this, &MainWindow::onAutoAnnotateUnannotated);
    connect(m_detector, &YoloDetector::modelLoaded, [this, unannotatedAction](const QString&) {
        m_autoAnnotateAction->setEnabled(true);
        m_batchAnnotateAction->setEnabled(true);
        unannotatedAction->setEnabled(true);
        // 检查模型类别数是否超过classes.txt
        if (m_classesLoader.classCount() > 0 && m_detector->numClasses() > m_classesLoader.classCount()) {
            QMessageBox::warning(this, "类别数量不匹配",
                QString("模型有 %1 个类别，但 classes.txt 只定义了 %2 个类别。\n\n"
                        "超出的类别将显示为 class_X 格式。\n"
                        "建议更新 classes.txt 文件。")
                .arg(m_detector->numClasses())
                .arg(m_classesLoader.classCount()));
        }
    });
    connect(m_detector, &YoloDetector::modelUnloaded, [this, unannotatedAction]() {
        m_autoAnnotateAction->setEnabled(false);
        m_batchAnnotateAction->setEnabled(false);
        unannotatedAction->setEnabled(false);
    });
}

void MainWindow::setupToolBar() {
    QToolBar* toolbar = addToolBar("工具栏"); toolbar->setMovable(false);
    QAction* prevAction = toolbar->addAction("上一张 (A)");
    prevAction->setShortcut(QKeySequence("A"));
    connect(prevAction, &QAction::triggered, this, &MainWindow::onPrevImage);
    QAction* nextAction = toolbar->addAction("下一张 (D)");
    nextAction->setShortcut(QKeySequence("D"));
    connect(nextAction, &QAction::triggered, this, &MainWindow::onNextImage);
    toolbar->addSeparator();
    QActionGroup* modeGroup = new QActionGroup(this);
    m_selectAction = toolbar->addAction("选择 (V)");
    m_selectAction->setCheckable(true); m_selectAction->setChecked(true);
    m_selectAction->setShortcut(QKeySequence("V")); modeGroup->addAction(m_selectAction);
    m_bboxAction = toolbar->addAction("边界框 (B)");
    m_bboxAction->setCheckable(true);
    m_bboxAction->setShortcut(QKeySequence("B")); modeGroup->addAction(m_bboxAction);
    m_keypointAction = toolbar->addAction("关键点 (K)");
    m_keypointAction->setCheckable(true);
    m_keypointAction->setShortcut(QKeySequence("K")); modeGroup->addAction(m_keypointAction);
    connect(modeGroup, &QActionGroup::triggered, this, &MainWindow::onModeChanged);
    toolbar->addSeparator();
    QAction* saveAction = toolbar->addAction("保存 (Ctrl+S)");
    saveAction->setShortcut(QKeySequence("Ctrl+S"));
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSaveAnnotation);
    QAction* clearAction = toolbar->addAction("清空");
    connect(clearAction, &QAction::triggered, this, &MainWindow::onClearAnnotations);
    toolbar->addSeparator();
    m_autoSaveAction = toolbar->addAction("自动保存");
    m_autoSaveAction->setCheckable(true);
    m_autoSaveAction->setChecked(m_autoSave);
    connect(m_autoSaveAction, &QAction::toggled, [this](bool checked) { m_autoSave = checked; });

    // 自动标注工具栏
    toolbar->addSeparator();
    toolbar->addWidget(new QLabel(" 自动标注: "));
    m_annotateModeCombo = new QComboBox();
    m_annotateModeCombo->addItem("追加");
    m_annotateModeCombo->addItem("覆盖");
    m_annotateModeCombo->setToolTip("选择标注模式:\n追加 - 保留现有标注\n覆盖 - 清除后重新标注");
    m_annotateModeCombo->setFixedWidth(60);
    toolbar->addWidget(m_annotateModeCombo);
    m_quickAnnotateAction = toolbar->addAction("识别 (G)");
    m_quickAnnotateAction->setShortcut(QKeySequence("G"));
    m_quickAnnotateAction->setToolTip("对当前图片进行自动标注 (G)");
    connect(m_quickAnnotateAction, &QAction::triggered, this, &MainWindow::onAutoAnnotateCurrent);
    m_stopBatchAction = toolbar->addAction("停止");
    m_stopBatchAction->setToolTip("停止批量标注");
    m_stopBatchAction->setVisible(false);
    connect(m_stopBatchAction, &QAction::triggered, this, &MainWindow::onStopBatchAnnotate);
}

void MainWindow::setupConnections() {
    connect(m_fileList, &QListWidget::itemClicked, this, &MainWindow::onFileSelected);
    connect(m_classList, &QListWidget::itemClicked, this, &MainWindow::onClassItemClicked);
    connect(m_annotationList, &QListWidget::currentRowChanged, m_canvasView, &CanvasView::setSelectedAnnotation);
    connect(m_canvasView, &CanvasView::annotationsChanged, this, &MainWindow::onAnnotationChanged);
    connect(m_canvasView, &CanvasView::annotationSelected, this, &MainWindow::onAnnotationSelected);
    connect(m_canvasView, &CanvasView::annotationDoubleClicked, this, &MainWindow::onAnnotationDoubleClicked);
    connect(m_canvasView, &CanvasView::zoomChanged, this, &MainWindow::onZoomChanged);
    connect(m_canvasView, &CanvasView::classSelectRequested, this, &MainWindow::onClassSelectRequested);
    connect(m_canvasView, &CanvasView::contextMenuRequested, this, &MainWindow::onContextMenuRequested);
    connect(m_classList, &QListWidget::itemDoubleClicked, this, &MainWindow::onClassListDoubleClicked);
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int i){ m_annotationIO.setFormat(i==0 ? AnnotationIO::Detection : AnnotationIO::Pose); });
}

void MainWindow::onOpenFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择图片文件夹", m_currentFolder);
    if (dir.isEmpty()) return;
    if (m_modified && m_autoSave) saveCurrentAnnotation();

    // 优先检测 YAML 项目文件
    QString yamlPath = findYamlInFolder(dir);
    if (!yamlPath.isEmpty()) {
        int ret = QMessageBox::question(this, "发现项目文件",
            QString("找到项目文件:\n%1\n\n是否作为项目打开?").arg(yamlPath),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (ret == QMessageBox::Cancel) return;
        if (ret == QMessageBox::Yes) {
            DatasetConfig config;
            if (config.loadYAML(yamlPath)) {
                loadProject(config);
                return;
            }
        }
    }

    // 传统方式打开文件夹
    m_currentFolder = dir; m_imageFiles.clear(); m_classesLocked = false;
    m_classesLoader.clear();  // 清除旧的类别数据
    m_datasetConfig.clear();  // 清除项目配置
    QDir directory(dir);
    QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.bmp"};
    m_imageFiles = directory.entryList(filters, QDir::Files, QDir::Name);
    if (m_imageFiles.isEmpty()) { QMessageBox::warning(this, "警告", "所选文件夹中没有图片文件!"); return; }
    QString classesPath = dir + "/classes.txt";
    QDir parentDir(dir); parentDir.cdUp();
    QString parentClassesPath = parentDir.absolutePath() + "/classes.txt";
    if (QFile::exists(classesPath)) {
        int ret = QMessageBox::question(this, "类别文件", QString("找到类别文件:\n%1\n\n是否使用此文件?").arg(classesPath), QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel) return;
        if (ret == QMessageBox::No) { classesPath = QFileDialog::getOpenFileName(this, "选择类别文件", dir, "文本文件 (*.txt)"); if (classesPath.isEmpty()) return; }
    } else if (QFile::exists(parentClassesPath)) {
        int ret = QMessageBox::question(this, "类别文件", QString("找到类别文件:\n%1\n\n是否使用此文件?").arg(parentClassesPath), QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel) return;
        if (ret == QMessageBox::Yes) classesPath = parentClassesPath;
        else { classesPath = QFileDialog::getOpenFileName(this, "选择类别文件", dir, "文本文件 (*.txt)"); if (classesPath.isEmpty()) return; }
    } else {
        int ret = QMessageBox::question(this, "类别文件", "未找到 classes.txt 文件.\n\n是否选择类别文件?", QMessageBox::Yes|QMessageBox::No);
        if (ret == QMessageBox::Yes) { classesPath = QFileDialog::getOpenFileName(this, "选择类别文件", dir, "文本文件 (*.txt)"); if (classesPath.isEmpty()) return; }
        else classesPath.clear();
    }
    if (!classesPath.isEmpty() && QFile::exists(classesPath)) { m_classesLoader.load(classesPath); m_classesLocked = true; }
    updateClassList();
    QString labelsDir;
    if (dir.contains("/images") || dir.contains("\\images")) {
        QString possibleLabels = dir;
        possibleLabels.replace("/images", "/labels"); possibleLabels.replace("\\images", "\\labels");
        if (QDir(possibleLabels).exists()) labelsDir = possibleLabels;
    }
    if (labelsDir.isEmpty()) { QDir parent(dir); parent.cdUp(); QString siblingLabels = parent.absolutePath() + "/labels"; if (QDir(siblingLabels).exists()) labelsDir = siblingLabels; }
    bool hasTxtInSameDir = false;
    for (const QString& img : m_imageFiles) { if (QFile::exists(dir + "/" + QFileInfo(img).completeBaseName() + ".txt")) { hasTxtInSameDir = true; break; } }
    if (!labelsDir.isEmpty()) {
        int ret = QMessageBox::question(this, "标签文件夹", QString("找到标签文件夹:\n%1\n\n是否使用此文件夹?").arg(labelsDir), QMessageBox::Yes|QMessageBox::No);
        if (ret == QMessageBox::No) labelsDir = QFileDialog::getExistingDirectory(this, "选择标签文件夹", dir);
    } else if (hasTxtInSameDir) {
        int ret = QMessageBox::question(this, "标签文件夹", "图片目录中已有标签文件(.txt).\n\n是否使用图片同目录存储标签?", QMessageBox::Yes|QMessageBox::No);
        if (ret == QMessageBox::Yes) labelsDir = dir; else labelsDir = QFileDialog::getExistingDirectory(this, "选择标签文件夹", dir);
    } else {
        int ret = QMessageBox::question(this, "标签文件夹", "未找到标签文件夹.\n\n是否选择标签文件夹?\n(选\"否\"将在图片同目录创建标签)", QMessageBox::Yes|QMessageBox::No);
        if (ret == QMessageBox::Yes) labelsDir = QFileDialog::getExistingDirectory(this, "选择标签文件夹", dir);
    }
    if (labelsDir.isEmpty()) labelsDir = dir;
    QDir().mkpath(labelsDir); m_labelsFolder = labelsDir;
    updateFileList();
    if (!m_imageFiles.isEmpty()) loadImage(0);
    setWindowTitle(QString("YOLO 标注工具 - %1").arg(dir));
}

void MainWindow::onOpenImage() {
    QString file = QFileDialog::getOpenFileName(this, "打开图片", m_currentFolder, "图片文件 (*.jpg *.jpeg *.png *.bmp)");
    if (file.isEmpty()) return;
    if (m_modified && m_autoSave) saveCurrentAnnotation();
    m_canvasView->setImage(file);
    QString txtPath = AnnotationIO::getAnnotationPath(file);
    if (QFile::exists(txtPath)) m_canvasView->setAnnotations(m_annotationIO.load(txtPath));
    else m_canvasView->clearAnnotations();
    m_modified = false; updateAnnotationList(); updateStatusBar();
}

void MainWindow::onImportClasses() {
    if (m_classesLocked) { QMessageBox::information(this, "提示", "类别已锁定,无法修改.\n请重新打开文件夹以更改类别."); return; }
    QString file = QFileDialog::getOpenFileName(this, "导入类别文件", m_currentFolder, "文本文件 (*.txt)");
    if (file.isEmpty()) return;
    m_classesLoader.load(file); m_canvasView->setClassNames(m_classesLoader.classNames());
    m_classesLocked = true; updateClassList();
}

void MainWindow::onSaveAnnotation() { saveCurrentAnnotation(); m_modified = false; updateStatusBar(); }

void MainWindow::onFileSelected(QListWidgetItem* item) {
    int index = m_fileList->row(item);
    if (index >= 0 && index != m_currentIndex) {
        if (!checkUnsavedChanges()) {
            // 用户取消，恢复列表选择
            m_fileList->blockSignals(true);
            m_fileList->setCurrentRow(m_currentIndex);
            m_fileList->blockSignals(false);
            return;
        }
        loadImage(index);
    }
}

void MainWindow::onPrevImage() {
    if (m_currentIndex > 0 && checkUnsavedChanges()) {
        loadImage(m_currentIndex - 1);
    }
}

void MainWindow::onNextImage() {
    if (m_currentIndex < m_imageFiles.size() - 1 && checkUnsavedChanges()) {
        loadImage(m_currentIndex + 1);
    }
}
void MainWindow::onAnnotationChanged() { m_modified = true; updateAnnotationList(); updateStatusBar(); }
void MainWindow::onAnnotationSelected(int index) { m_annotationList->blockSignals(true); m_annotationList->setCurrentRow(index); m_annotationList->blockSignals(false); }
void MainWindow::onAnnotationDoubleClicked(int index) { Q_UNUSED(index); }
void MainWindow::onDeleteAnnotation() { int index = m_canvasView->selectedAnnotation(); if (index >= 0) m_canvasView->removeAnnotation(index); }

void MainWindow::onClearAnnotations() {
    if (m_canvasView->annotations().isEmpty()) return;
    int ret = QMessageBox::question(this, "确认清空", "确定要清空当前图片的所有标注吗?", QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        m_canvasView->clearAnnotations();
    }
}

void MainWindow::onClassItemClicked(QListWidgetItem* item) {
    int classId = m_classList->row(item);
    m_canvasView->setCurrentClass(classId);
    int selectedIdx = m_canvasView->selectedAnnotation();
    if (selectedIdx >= 0 && selectedIdx < m_canvasView->annotations().size()) {
        Annotation ann = m_canvasView->annotations()[selectedIdx];
        if (ann.classId() != classId) { 
            ann.setClassId(classId); 
            m_canvasView->updateAnnotation(selectedIdx, ann); 
        }
    }
}

void MainWindow::onModeChanged(QAction* action) {
    if (action == m_selectAction) m_canvasView->setEditMode(EditMode::Select);
    else if (action == m_bboxAction) m_canvasView->setEditMode(EditMode::DrawBBox);
    else if (action == m_keypointAction) m_canvasView->setEditMode(EditMode::DrawKeypoint);
}

void MainWindow::onZoomChanged(double scale) { m_zoomLabel->setText(QString("%1%").arg(int(scale * 100))); }

void MainWindow::onEditSkeletonConfig() {
    SkeletonConfigDialog dialog(this);
    dialog.setConfig(m_canvasView->skeletonConfig());
    dialog.setProjectPath(m_datasetConfig.projectPath());  // 传递项目路径

    if (dialog.exec() == QDialog::Accepted) {
        SkeletonConfig config = dialog.config();
        m_canvasView->setSkeletonConfig(config);
        m_annotationIO.setKeypointCount(config.keypointCount());

        // 更新项目配置
        m_datasetConfig.setSkeletonConfig(config);
        m_datasetConfig.setKptShape(config.keypointCount(), 3);

        if (config.keypointCount() > 0) {
            m_formatCombo->setCurrentIndex(1);  // Pose 模式
        }
    }
}

void MainWindow::loadImage(int index) {
    if (index < 0 || index >= m_imageFiles.size()) return;
    m_currentIndex = index;
    QString imagePath = m_currentFolder + "/" + m_imageFiles[index];
    m_canvasView->setClassNames(m_classesLoader.classNames());  // 确保类别名称同步
    m_canvasView->setImage(imagePath);
    QString txtPath = getAnnotationPath(imagePath);
    if (QFile::exists(txtPath)) {
        int kpCount = 0, kpDim = 3;
        auto format = AnnotationIO::detectFormat(txtPath, kpCount, kpDim);
        m_annotationIO.setFormat(format); m_annotationIO.setKeypointCount(kpCount); m_annotationIO.setKptDim(kpDim);
        m_formatCombo->setCurrentIndex(format == AnnotationIO::Detection ? 0 : 1);
        m_canvasView->setAnnotations(m_annotationIO.load(txtPath));
    } else m_canvasView->clearAnnotations();
    m_fileList->setCurrentRow(index); m_modified = false;
    updateAnnotationList(); updateStatusBar();
}

void MainWindow::saveCurrentAnnotation() {
    if (m_currentIndex < 0 || m_currentIndex >= m_imageFiles.size()) return;
    QString imagePath = m_currentFolder + "/" + m_imageFiles[m_currentIndex];
    m_annotationIO.save(getAnnotationPath(imagePath), m_canvasView->annotations());
    m_modified = false;
    // 更新文件列表显示
    if (QListWidgetItem* item = m_fileList->item(m_currentIndex)) {
        bool hasAnnotation = !m_canvasView->annotations().isEmpty();
        QString file = m_imageFiles[m_currentIndex];
        item->setText(hasAnnotation ? QString::fromUtf8("✓ ") + file : QString::fromUtf8("   ") + file);
        item->setForeground(hasAnnotation ? QColor(0, 180, 0) : palette().text().color());
    }
}

bool MainWindow::checkUnsavedChanges() {
    if (!m_modified) return true;  // 没有修改，直接返回

    if (m_autoSave) {
        saveCurrentAnnotation();
        return true;
    }

    // 用户选择了不再提醒
    if (m_skipSaveReminder) {
        if (m_skipSaveAction) {
            saveCurrentAnnotation();
        } else {
            m_modified = false;
        }
        return true;
    }

    // 创建自定义对话框
    QDialog dialog(this);
    dialog.setWindowTitle("未保存的更改");

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QLabel* label = new QLabel("当前图片的标注尚未保存，是否保存？");
    layout->addWidget(label);

    QCheckBox* checkBox = new QCheckBox("不再提醒，以后都执行相同操作");
    layout->addWidget(checkBox);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* saveBtn = new QPushButton("保存");
    QPushButton* discardBtn = new QPushButton("放弃");
    QPushButton* cancelBtn = new QPushButton("取消");
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(discardBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    int result = -1;  // 0=保存, 1=放弃, -1=取消
    connect(saveBtn, &QPushButton::clicked, [&]() { result = 0; dialog.accept(); });
    connect(discardBtn, &QPushButton::clicked, [&]() { result = 1; dialog.accept(); });
    connect(cancelBtn, &QPushButton::clicked, [&]() { dialog.reject(); });

    saveBtn->setDefault(true);
    dialog.exec();

    if (result == 0) {
        if (checkBox->isChecked()) {
            // 相当于开启自动保存
            m_autoSave = true;
            m_autoSaveAction->setChecked(true);
        }
        saveCurrentAnnotation();
        return true;
    } else if (result == 1) {
        if (checkBox->isChecked()) {
            m_skipSaveReminder = true;
            m_skipSaveAction = false;
        }
        m_modified = false;
        return true;
    }
    return false;  // 取消
}

void MainWindow::updateFileList() {
    // 停止之前的后台检查
    stopStatusCheck();

    // 快速加载：只显示文件名，不检查标注状态
    m_fileList->blockSignals(true);
    m_fileList->clear();
    for (const QString& file : m_imageFiles) {
        QListWidgetItem* item = new QListWidgetItem(QString::fromUtf8("   ") + file);
        m_fileList->addItem(item);
    }
    m_fileList->blockSignals(false);

    // 启动后台检查标注状态
    startStatusCheck();
}

void MainWindow::startStatusCheck() {
    if (m_imageFiles.isEmpty()) return;

    m_statusThread = new QThread(this);
    m_statusWorker = new AnnotationStatusWorker();
    m_statusWorker->setFiles(m_imageFiles, m_currentFolder, m_labelsFolder);
    m_statusWorker->moveToThread(m_statusThread);

    connect(m_statusThread, &QThread::started, m_statusWorker, &AnnotationStatusWorker::process);
    connect(m_statusWorker, &AnnotationStatusWorker::fileStatusReady, this, &MainWindow::onFileStatusReady);
    connect(m_statusWorker, &AnnotationStatusWorker::finished, this, &MainWindow::onStatusCheckFinished);
    connect(m_statusWorker, &AnnotationStatusWorker::finished, m_statusThread, &QThread::quit);
    connect(m_statusThread, &QThread::finished, m_statusWorker, &QObject::deleteLater);
    connect(m_statusThread, &QThread::finished, m_statusThread, &QObject::deleteLater);

    m_statusThread->start();
}

void MainWindow::stopStatusCheck() {
    if (m_statusWorker) {
        m_statusWorker->requestStop();
    }
    if (m_statusThread && m_statusThread->isRunning()) {
        m_statusThread->quit();
        m_statusThread->wait(1000);
    }
    m_statusWorker = nullptr;
    m_statusThread = nullptr;
}

void MainWindow::onFileStatusReady(int index, bool hasAnnotation) {
    if (index < 0 || index >= m_fileList->count()) return;

    QListWidgetItem* item = m_fileList->item(index);
    QString fileName = m_imageFiles[index];

    if (hasAnnotation) {
        item->setText(QString::fromUtf8("✓ ") + fileName);
        item->setForeground(QColor(0, 180, 0));
    }
}

void MainWindow::onStatusCheckFinished() {
    // 后台检查完成，可以在这里做一些清理或提示
}

void MainWindow::updateAnnotationList() {
    int selectedIdx = m_canvasView->selectedAnnotation();
    m_annotationList->blockSignals(true);
    m_annotationList->clear();
    const auto& annotations = m_canvasView->annotations();
    for (int i = 0; i < annotations.size(); ++i)
        m_annotationList->addItem(QString("[%1] %2").arg(i).arg(m_classesLoader.className(annotations[i].classId())));
    if (selectedIdx >= 0 && selectedIdx < annotations.size())
        m_annotationList->setCurrentRow(selectedIdx);
    m_annotationList->blockSignals(false);
}

void MainWindow::updateClassList() {
    m_classList->clear();
    for (int i = 0; i < m_classesLoader.classCount(); ++i) {
        QListWidgetItem* item = new QListWidgetItem(m_classesLoader.className(i));
        item->setForeground(m_classesLoader.classColor(i)); m_classList->addItem(item);
    }
    m_canvasView->setClassNames(m_classesLoader.classNames());
}

void MainWindow::updateStatusBar() {
    QString status;
    if (m_currentIndex >= 0 && m_currentIndex < m_imageFiles.size()) {
        status = QString("%1 | %2/%3 | %4 个标注").arg(m_imageFiles[m_currentIndex]).arg(m_currentIndex+1).arg(m_imageFiles.size()).arg(m_canvasView->annotations().size());
        if (m_modified) status += " *";
    } else status = "就绪";
    m_statusLabel->setText(status);
}

void MainWindow::onClassSelectRequested(int annotationIndex, QPoint globalPos) {
    if (m_classesLoader.classCount() == 0) return;

    // 使用 QMenu 在鼠标位置显示类别选择
    QMenu menu;
    for (int i = 0; i < m_classesLoader.classCount(); ++i) {
        QAction* action = menu.addAction(QString("[%1] %2").arg(i).arg(m_classesLoader.className(i)));
        action->setData(i);
    }

    QAction* selected = menu.exec(globalPos);
    if (selected) {
        int classId = selected->data().toInt();
        if (annotationIndex >= 0 && annotationIndex < m_canvasView->annotations().size()) {
            Annotation ann = m_canvasView->annotations()[annotationIndex];
            ann.setClassId(classId);
            m_canvasView->updateAnnotation(annotationIndex, ann);
            m_canvasView->setCurrentClass(classId);
            m_classList->setCurrentRow(classId);
        }
    } else {
        // 用户取消，删除刚创建的标注
        if (annotationIndex >= 0 && annotationIndex < m_canvasView->annotations().size()) {
            m_canvasView->removeAnnotation(annotationIndex);
        }
    }
}

void MainWindow::onClassListDoubleClicked(QListWidgetItem* item) {
    int classId = m_classList->row(item);
    int selectedIdx = m_canvasView->selectedAnnotation();
    if (selectedIdx >= 0 && selectedIdx < m_canvasView->annotations().size()) {
        Annotation ann = m_canvasView->annotations()[selectedIdx];
        ann.setClassId(classId); m_canvasView->updateAnnotation(selectedIdx, ann);
        m_canvasView->setCurrentClass(classId);
    }
}

QString MainWindow::getAnnotationPath(const QString& imagePath) {
    QFileInfo info(imagePath);
    QString baseName = info.completeBaseName();
    if (!m_labelsFolder.isEmpty()) return m_labelsFolder + "/" + baseName + ".txt";
    return info.absolutePath() + "/" + baseName + ".txt";
}

void MainWindow::onContextMenuRequested(int annotationIndex, const QPoint& globalPos) {
    if (annotationIndex < 0 || annotationIndex >= m_canvasView->annotations().size()) return;

    QMenu menu(this);

    // 更改类别子菜单
    QMenu* classMenu = menu.addMenu("更改类别");
    for (int i = 0; i < m_classesLoader.classCount(); ++i) {
        QAction* action = classMenu->addAction(QString("[%1] %2").arg(i).arg(m_classesLoader.className(i)));
        action->setData(i);
    }

    menu.addSeparator();
    QAction* deleteAction = menu.addAction("删除");

    QAction* selected = menu.exec(globalPos);
    if (selected == deleteAction) {
        m_canvasView->removeAnnotation(annotationIndex);
    } else if (selected && selected->data().isValid()) {
        int classId = selected->data().toInt();
        Annotation ann = m_canvasView->annotations()[annotationIndex];
        ann.setClassId(classId);
        m_canvasView->updateAnnotation(annotationIndex, ann);
        m_canvasView->setCurrentClass(classId);
        m_classList->setCurrentRow(classId);
    }
}

void MainWindow::onModelSettings() {
    ModelSettingsDialog dialog(m_detector, this);
    dialog.setDatasetClasses(m_classesLoader.classNames());
    dialog.exec();
}

void MainWindow::onAutoAnnotateCurrent() {
    if (!m_detector->isLoaded()) {
        QMessageBox::warning(this, "警告", "请先加载ONNX模型\n(自动标注 -> 模型设置)");
        return;
    }
    if (m_canvasView->currentImage().isNull()) {
        QMessageBox::warning(this, "警告", "请先打开图片");
        return;
    }

    // 更新状态显示
    m_statusLabel->setText("正在识别...");

    // 更新文件列表当前项状态
    if (m_currentIndex >= 0 && m_currentIndex < m_fileList->count()) {
        QListWidgetItem* item = m_fileList->item(m_currentIndex);
        item->setText(QString::fromUtf8("⏳ ") + m_imageFiles[m_currentIndex]);
        item->setForeground(QColor(255, 165, 0));  // 橙色表示识别中
    }
    QApplication::processEvents();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto annotations = m_detector->detect(m_canvasView->currentImage());
    QApplication::restoreOverrideCursor();

    // 根据工具栏的模式决定是否清除
    bool overwriteMode = m_annotateModeCombo->currentIndex() == 1;  // 1=覆盖

    if (annotations.isEmpty()) {
        m_statusLabel->setText("未检测到目标");
        // 恢复文件列表状态
        if (m_currentIndex >= 0 && m_currentIndex < m_fileList->count()) {
            QString imgPath = m_currentFolder + "/" + m_imageFiles[m_currentIndex];
            bool hasAnnotation = QFile::exists(getAnnotationPath(imgPath)) || !m_canvasView->annotations().isEmpty();
            QListWidgetItem* item = m_fileList->item(m_currentIndex);
            item->setText((hasAnnotation ? QString::fromUtf8("✓ ") : "   ") + m_imageFiles[m_currentIndex]);
            item->setForeground(hasAnnotation ? QColor(0, 180, 0) : palette().text().color());
        }
        return;
    }

    if (overwriteMode) {
        m_canvasView->clearAnnotations();
    }

    for (const auto& ann : annotations) {
        m_canvasView->addAnnotation(ann);
    }

    // 更新文件列表当前项状态（已标注）
    if (m_currentIndex >= 0 && m_currentIndex < m_fileList->count()) {
        QListWidgetItem* item = m_fileList->item(m_currentIndex);
        item->setText(QString::fromUtf8("✓ ") + m_imageFiles[m_currentIndex]);
        item->setForeground(QColor(0, 180, 0));
    }

    m_statusLabel->setText(QString("检测到 %1 个目标 [%2]")
        .arg(annotations.size())
        .arg(overwriteMode ? "覆盖" : "追加"));
}

void MainWindow::onAutoAnnotateBatch() {
    if (!m_detector->isLoaded()) {
        QMessageBox::warning(this, "警告", "请先加载ONNX模型");
        return;
    }
    if (m_imageFiles.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先打开文件夹");
        return;
    }
    if (m_batchRunning) {
        QMessageBox::information(this, "提示", "批量标注正在进行中");
        return;
    }

    bool overwriteMode = m_annotateModeCombo->currentIndex() == 1;
    auto reply = QMessageBox::question(this, "批量自动标注",
        QString("将对 %1 张图片进行自动标注\n模式: %2\n\n是否继续?")
            .arg(m_imageFiles.size())
            .arg(overwriteMode ? "覆盖现有标注" : "追加到现有标注"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // 保存当前
    if (m_modified && m_autoSave) saveCurrentAnnotation();

    // 创建后台线程
    m_batchThread = new QThread(this);
    m_batchWorker = new BatchAnnotateWorker(m_detector, &m_annotationIO);
    m_batchWorker->setFiles(m_imageFiles, m_currentFolder, m_labelsFolder, overwriteMode);
    m_batchWorker->moveToThread(m_batchThread);

    connect(m_batchThread, &QThread::started, m_batchWorker, &BatchAnnotateWorker::process);
    connect(m_batchWorker, &BatchAnnotateWorker::progressUpdated, this, &MainWindow::onBatchProgress);
    connect(m_batchWorker, &BatchAnnotateWorker::fileCompleted, this, &MainWindow::onBatchFileCompleted);
    connect(m_batchWorker, &BatchAnnotateWorker::finished, this, &MainWindow::onBatchFinished);
    connect(m_batchWorker, &BatchAnnotateWorker::finished, m_batchThread, &QThread::quit);
    connect(m_batchThread, &QThread::finished, m_batchWorker, &QObject::deleteLater);
    connect(m_batchThread, &QThread::finished, m_batchThread, &QObject::deleteLater);

    m_batchRunning = true;
    m_stopBatchAction->setVisible(true);
    m_batchAnnotateAction->setEnabled(false);

    m_batchThread->start();
}

void MainWindow::onBatchProgress(int index, int total, const QString& fileName, int) {
    // 更新文件列表状态 - 正在处理
    int fileIdx = m_imageFiles.indexOf(fileName);
    if (fileIdx >= 0 && fileIdx < m_fileList->count()) {
        QListWidgetItem* item = m_fileList->item(fileIdx);
        item->setText(QString::fromUtf8("⏳ ") + fileName);
        item->setForeground(QColor(255, 165, 0));
    }

    // 更新状态栏
    bool overwriteMode = m_annotateModeCombo->currentIndex() == 1;
    int percent = (index + 1) * 100 / total;
    m_statusLabel->setText(QString("批量标注[%1]: %2 (%3/%4) %5%")
        .arg(overwriteMode ? "覆盖" : "追加")
        .arg(fileName).arg(index + 1).arg(total).arg(percent));
}

void MainWindow::onBatchFileCompleted(int, const QString& fileName, int detections) {
    // 更新文件列表状态 - 根据是否检测到目标决定颜色
    int fileIdx = m_imageFiles.indexOf(fileName);
    if (fileIdx >= 0 && fileIdx < m_fileList->count()) {
        QListWidgetItem* item = m_fileList->item(fileIdx);
        if (detections > 0) {
            // 检测到目标，标绿
            item->setText(QString::fromUtf8("✓ ") + fileName);
            item->setForeground(QColor(0, 180, 0));
        } else {
            // 未检测到目标，恢复默认
            item->setText("   " + fileName);
            item->setForeground(palette().text().color());
        }
    }
}

void MainWindow::onBatchFinished(int totalProcessed, int totalDetections) {
    m_batchRunning = false;
    m_stopBatchAction->setVisible(false);
    m_batchAnnotateAction->setEnabled(true);
    m_batchThread = nullptr;
    m_batchWorker = nullptr;

    // 刷新当前图片
    if (m_currentIndex >= 0) {
        loadImage(m_currentIndex);
    }

    if (totalProcessed >= 0) {
        m_statusLabel->setText(QString("批量标注完成: %1 张, 检测到 %2 个目标")
            .arg(totalProcessed).arg(totalDetections));
    } else {
        m_statusLabel->setText("批量标注已停止");
    }
}

void MainWindow::onStopBatchAnnotate() {
    if (m_batchWorker) {
        m_batchWorker->requestStop();
    }
    m_statusLabel->setText("正在停止...");
}

void MainWindow::onAutoAnnotateUnannotated() {
    if (!m_detector->isLoaded()) {
        QMessageBox::warning(this, "警告", "请先加载ONNX模型");
        return;
    }
    if (m_imageFiles.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先打开文件夹");
        return;
    }
    if (m_batchRunning) {
        QMessageBox::information(this, "提示", "批量标注正在进行中");
        return;
    }

    // 找出未标注的图片
    QStringList unannotated;
    for (const QString& file : m_imageFiles) {
        QString imagePath = m_currentFolder + "/" + file;
        QString txtPath = getAnnotationPath(imagePath);
        if (!QFile::exists(txtPath)) {
            unannotated.append(file);
        }
    }

    if (unannotated.isEmpty()) {
        QMessageBox::information(this, "提示", "所有图片都已有标注");
        return;
    }

    auto reply = QMessageBox::question(this, "标注未标注图片",
        QString("找到 %1 张未标注图片\n\n是否进行自动标注?").arg(unannotated.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // 保存当前
    if (m_modified && m_autoSave) saveCurrentAnnotation();

    // 创建后台线程（未标注的图片使用覆盖模式）
    m_batchThread = new QThread(this);
    m_batchWorker = new BatchAnnotateWorker(m_detector, &m_annotationIO);
    m_batchWorker->setFiles(unannotated, m_currentFolder, m_labelsFolder, true);
    m_batchWorker->moveToThread(m_batchThread);

    connect(m_batchThread, &QThread::started, m_batchWorker, &BatchAnnotateWorker::process);
    connect(m_batchWorker, &BatchAnnotateWorker::progressUpdated, this, &MainWindow::onBatchProgress);
    connect(m_batchWorker, &BatchAnnotateWorker::fileCompleted, this, &MainWindow::onBatchFileCompleted);
    connect(m_batchWorker, &BatchAnnotateWorker::finished, this, &MainWindow::onBatchFinished);
    connect(m_batchWorker, &BatchAnnotateWorker::finished, m_batchThread, &QThread::quit);
    connect(m_batchThread, &QThread::finished, m_batchWorker, &QObject::deleteLater);
    connect(m_batchThread, &QThread::finished, m_batchThread, &QObject::deleteLater);

    m_batchRunning = true;
    m_stopBatchAction->setVisible(true);
    m_batchAnnotateAction->setEnabled(false);

    m_batchThread->start();
}

// ==================== 项目操作 ====================

void MainWindow::onOpenProject() {
    QString path = QFileDialog::getOpenFileName(this, "打开项目文件",
        m_currentFolder.isEmpty() ? QString() : m_currentFolder,
        "YAML 项目文件 (*.yaml *.yml);;所有文件 (*)");

    if (path.isEmpty()) return;

    if (m_modified && m_autoSave) saveCurrentAnnotation();

    DatasetConfig config;
    if (!config.loadYAML(path)) {
        QMessageBox::warning(this, "错误", "无法解析项目文件:\n" + path);
        return;
    }

    loadProject(config);
}

void MainWindow::onCreateProjectFromTxt() {
    QString txtPath = QFileDialog::getOpenFileName(this, "选择 classes.txt 文件",
        m_currentFolder.isEmpty() ? QString() : m_currentFolder,
        "类别文件 (*.txt);;所有文件 (*)");

    if (txtPath.isEmpty()) return;

    // 加载类别
    QStringList classNames;
    QFile file(txtPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                classNames.append(line);
            }
        }
    }

    if (classNames.isEmpty()) {
        QMessageBox::warning(this, "错误", "classes.txt 文件为空或无法读取");
        return;
    }

    // 显示新建项目对话框
    NewProjectDialog dialog(this);
    dialog.setClassNames(classNames);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    DatasetConfig config = dialog.config();
    QString savePath = dialog.projectSavePath();
    QString imagePath = dialog.imageFolderPath();

    // 保存 YAML 文件
    if (!config.saveYAML(savePath)) {
        QMessageBox::warning(this, "错误", "无法保存项目文件:\n" + savePath);
        return;
    }

    // 加载项目（使用已知的图片路径）
    config.loadYAML(savePath);  // 重新加载以获取正确的路径
    loadProject(config, imagePath);

    QMessageBox::information(this, "成功", "项目已创建:\n" + savePath);
}

void MainWindow::loadProject(const DatasetConfig& config, const QString& knownImagePath) {
    m_datasetConfig = config;
    m_classesLoader.setClassNames(config.classNames());
    m_classesLocked = !config.classNames().isEmpty();

    bool configModified = false;

    // 如果类别名称为空，让用户选择导入
    if (config.classNames().isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "类别名称",
            "YAML 文件中未找到类别名称 (names)。\n\n"
            "是否导入 classes.txt 文件?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        if (reply == QMessageBox::Yes) {
            QString txtPath = QFileDialog::getOpenFileName(this, "选择 classes.txt",
                QFileInfo(config.projectPath()).absolutePath(),
                "类别文件 (*.txt);;所有文件 (*)");

            if (!txtPath.isEmpty()) {
                m_classesLoader.load(txtPath);
                m_classesLocked = true;
                m_datasetConfig.setClassNames(m_classesLoader.classNames());
                configModified = true;
            }
        }
    }

    // 设置任务类型和骨架配置
    if (config.isPose()) {
        m_formatCombo->setCurrentIndex(1);
        m_canvasView->setSkeletonConfig(config.skeletonConfig());
        m_annotationIO.setFormat(AnnotationIO::Pose);
        m_annotationIO.setKeypointCount(config.keypointCount());
        m_annotationIO.setKptDim(config.keypointDim());
    } else {
        m_formatCombo->setCurrentIndex(0);
        m_annotationIO.setFormat(AnnotationIO::Detection);
    }

    // 加载图片 - 优先使用已知路径
    QString imagePath = knownImagePath;
    bool imagePathUserSelected = false;
    if (imagePath.isEmpty()) {
        if (!config.resolvedTrainPath().isEmpty() && QDir(config.resolvedTrainPath()).exists()) {
            imagePath = config.resolvedTrainPath();
        } else if (!config.resolvedValPath().isEmpty() && QDir(config.resolvedValPath()).exists()) {
            imagePath = config.resolvedValPath();
        } else if (!config.datasetPath().isEmpty() && QDir(config.datasetPath()).exists()) {
            imagePath = config.datasetPath();
        }
    }

    if (imagePath.isEmpty()) {
        // 询问用户选择图片文件夹
        imagePath = QFileDialog::getExistingDirectory(this, "选择图片文件夹",
            QFileInfo(config.projectPath()).absolutePath());
        imagePathUserSelected = !imagePath.isEmpty();
    }

    if (!imagePath.isEmpty()) {
        // 更新配置中的路径（使用相对于YAML的路径）
        if (imagePathUserSelected && !config.projectPath().isEmpty()) {
            QDir yamlDir = QFileInfo(config.projectPath()).absoluteDir();
            QString relativePath = yamlDir.relativeFilePath(imagePath);
            m_datasetConfig.setDatasetPath(relativePath);
            m_datasetConfig.setTrainPath(relativePath);
            configModified = true;
        }
        loadImagesFromPath(imagePath);
    }

    // 如果配置被修改，询问是否保存
    if (configModified && !config.projectPath().isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "保存配置",
            "项目配置已更新。\n\n是否保存到 YAML 文件?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        if (reply == QMessageBox::Yes) {
            m_datasetConfig.saveYAML(config.projectPath());
        }
    }

    updateClassList();
    setWindowTitle(QString("YOLO 标注工具 - %1").arg(
        config.projectPath().isEmpty() ? m_currentFolder : QFileInfo(config.projectPath()).fileName()));
}

void MainWindow::loadImagesFromPath(const QString& imagePath) {
    m_currentFolder = imagePath;
    m_imageFiles.clear();

    QDir directory(imagePath);
    QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.bmp"};
    m_imageFiles = directory.entryList(filters, QDir::Files, QDir::Name);

    if (m_imageFiles.isEmpty()) {
        QMessageBox::warning(this, "警告", "所选文件夹中没有图片文件!");
        return;
    }

    // 设置标签文件夹
    QString labelsDir;
    if (imagePath.contains("/images") || imagePath.contains("\\images")) {
        QString possibleLabels = imagePath;
        possibleLabels.replace("/images", "/labels");
        possibleLabels.replace("\\images", "\\labels");
        if (QDir(possibleLabels).exists()) {
            labelsDir = possibleLabels;
        }
    }

    if (labelsDir.isEmpty()) {
        // 检查同级 labels 目录
        QDir parent(imagePath);
        parent.cdUp();
        QString siblingLabels = parent.absolutePath() + "/labels";
        if (QDir(siblingLabels).exists()) {
            labelsDir = siblingLabels;
        }
    }

    if (labelsDir.isEmpty()) {
        // 检查是否已有标注文件
        bool hasTxt = false;
        for (const QString& img : m_imageFiles) {
            if (QFile::exists(imagePath + "/" + QFileInfo(img).completeBaseName() + ".txt")) {
                hasTxt = true;
                break;
            }
        }
        if (hasTxt) {
            labelsDir = imagePath;
        }
    }

    if (labelsDir.isEmpty()) {
        // 没有找到标签目录，询问用户
        QMessageBox::StandardButton reply = QMessageBox::question(this, "标签目录",
            "未找到标签目录。\n\n"
            "是否选择已有的标签目录?\n"
            "(选择\"否\"将创建新的 labels 目录)",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        if (reply == QMessageBox::Yes) {
            labelsDir = QFileDialog::getExistingDirectory(this, "选择标签目录",
                QFileInfo(imagePath).absolutePath());
        }

        if (labelsDir.isEmpty()) {
            // 用户取消或选择创建新目录
            QDir parent(imagePath);
            parent.cdUp();
            labelsDir = parent.absolutePath() + "/labels";
            QDir().mkpath(labelsDir);
        }
    }

    m_labelsFolder = labelsDir;

    updateFileList();
    if (!m_imageFiles.isEmpty()) {
        loadImage(0);
    }
}

QString MainWindow::findYamlInFolder(const QString& folder) {
    QDir dir(folder);
    QStringList yamls = dir.entryList({"*.yaml", "*.yml"}, QDir::Files);
    if (!yamls.isEmpty()) {
        return dir.absoluteFilePath(yamls.first());
    }

    // 检查上级目录
    QDir parent(folder);
    parent.cdUp();
    yamls = parent.entryList({"*.yaml", "*.yml"}, QDir::Files);
    if (!yamls.isEmpty()) {
        return parent.absoluteFilePath(yamls.first());
    }

    return QString();
}

QString MainWindow::findClassesTxtInFolder(const QString& folder) {
    QString classesPath = folder + "/classes.txt";
    if (QFile::exists(classesPath)) {
        return classesPath;
    }

    // 检查上级目录
    QDir parent(folder);
    parent.cdUp();
    classesPath = parent.absolutePath() + "/classes.txt";
    if (QFile::exists(classesPath)) {
        return classesPath;
    }

    return QString();
}

void MainWindow::onSplitDataset() {
    SplitDatasetDialog dialog(this);

    // 如果已打开文件夹，自动填充路径
    if (!m_currentFolder.isEmpty()) {
        dialog.setSourcePath(m_currentFolder, m_labelsFolder);
    }

    dialog.exec();
}

void MainWindow::onDatasetAnalysis() {
    if (m_labelsFolder.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先打开一个数据集项目或文件夹");
        return;
    }

    DatasetAnalysisDialog dialog(this);
    dialog.setLabelsPath(m_labelsFolder);
    dialog.setClassNames(m_classesLoader.classNames());
    dialog.analyze();
    dialog.exec();
}
