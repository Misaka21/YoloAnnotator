#include "MainWindow.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDir>
#include <QFile>
#include <QShortcut>
#include <QInputDialog>
#include <QActionGroup>
#include <QProgressDialog>
#include <QApplication>
#include <QColorDialog>
#include "inference/YoloDetector.h"
#include "inference/ModelSettingsDialog.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_detector = new YoloDetector(this);
    setupUI(); setupMenus(); setupToolBar(); setupConnections();
    setWindowTitle("YOLO 标注工具"); resize(1400, 900);
}

MainWindow::~MainWindow() { if (m_modified && m_autoSave) saveCurrentAnnotation(); }

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
    m_currentFolder = dir; m_imageFiles.clear(); m_classesLocked = false;
    m_classesLoader.clear();  // 清除旧的类别数据
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
    if (index >= 0 && index != m_currentIndex) { if (m_modified && m_autoSave) saveCurrentAnnotation(); loadImage(index); }
}

void MainWindow::onPrevImage() { if (m_currentIndex > 0) { if (m_modified && m_autoSave) saveCurrentAnnotation(); loadImage(m_currentIndex - 1); } }
void MainWindow::onNextImage() { if (m_currentIndex < m_imageFiles.size() - 1) { if (m_modified && m_autoSave) saveCurrentAnnotation(); loadImage(m_currentIndex + 1); } }
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
    bool ok;
    int count = QInputDialog::getInt(this, "骨架配置", "关键点数量:", m_canvasView->skeletonConfig().keypointCount(), 0, 100, 1, &ok);
    if (ok) {
        SkeletonConfig config = SkeletonConfig::createCustom(count);
        m_canvasView->setSkeletonConfig(config); m_annotationIO.setKeypointCount(count);
        if (count > 0) m_formatCombo->setCurrentIndex(1);
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
        int kpCount = 0;
        auto format = AnnotationIO::detectFormat(txtPath, kpCount);
        m_annotationIO.setFormat(format); m_annotationIO.setKeypointCount(kpCount);
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
    // 更新文件列表显示
    if (QListWidgetItem* item = m_fileList->item(m_currentIndex)) {
        bool hasAnnotation = !m_canvasView->annotations().isEmpty();
        QString file = m_imageFiles[m_currentIndex];
        item->setText(hasAnnotation ? QString::fromUtf8("✓ ") + file : QString::fromUtf8("   ") + file);
        item->setForeground(hasAnnotation ? QColor(0, 180, 0) : palette().text().color());
    }
}

void MainWindow::updateFileList() {
    m_fileList->clear();
    for (const QString& file : m_imageFiles) {
        QString imgPath = m_currentFolder + "/" + file;
        QString txtPath = getAnnotationPath(imgPath);
        bool hasAnnotation = QFile::exists(txtPath);
        // 用符号表示是否有标注
        QString displayText = hasAnnotation ? QString::fromUtf8("✓ ") + file : QString::fromUtf8("   ") + file;
        QListWidgetItem* item = new QListWidgetItem(displayText);
        if (hasAnnotation) {
            item->setForeground(QColor(0, 180, 0));  // 绿色表示已标注
        }
        m_fileList->addItem(item);
    }
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

    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto annotations = m_detector->detect(m_canvasView->currentImage());
    QApplication::restoreOverrideCursor();

    if (annotations.isEmpty()) {
        QMessageBox::information(this, "提示", "未检测到任何目标");
        return;
    }

    // 询问替换或追加
    auto reply = QMessageBox::question(this, "自动标注",
        QString("检测到 %1 个目标\n\n替换当前标注? (否=追加)").arg(annotations.size()),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (reply == QMessageBox::Cancel) return;
    if (reply == QMessageBox::Yes) m_canvasView->clearAnnotations();

    for (const auto& ann : annotations) {
        m_canvasView->addAnnotation(ann);
    }

    m_statusLabel->setText(QString("自动标注完成: 检测到 %1 个目标").arg(annotations.size()));
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

    auto reply = QMessageBox::question(this, "批量自动标注",
        QString("将对 %1 张图片进行自动标注\n已有标注的图片将被覆盖\n\n是否继续?").arg(m_imageFiles.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // 保存当前
    if (m_modified && m_autoSave) saveCurrentAnnotation();

    QProgressDialog progress("正在自动标注...", "取消", 0, m_imageFiles.size(), this);
    progress.setWindowModality(Qt::WindowModal);

    int totalDetections = 0;
    for (int i = 0; i < m_imageFiles.size(); ++i) {
        if (progress.wasCanceled()) break;
        progress.setValue(i);
        progress.setLabelText(QString("正在处理: %1\n(%2/%3)").arg(m_imageFiles[i]).arg(i+1).arg(m_imageFiles.size()));
        QApplication::processEvents();

        QString imagePath = m_currentFolder + "/" + m_imageFiles[i];
        QImage image(imagePath);
        if (image.isNull()) continue;

        auto annotations = m_detector->detect(image);
        totalDetections += annotations.size();

        // 保存
        QString txtPath = getAnnotationPath(imagePath);
        m_annotationIO.save(txtPath, annotations);
    }
    progress.setValue(m_imageFiles.size());

    updateFileList();
    if (m_currentIndex >= 0) loadImage(m_currentIndex);

    QMessageBox::information(this, "完成",
        QString("批量标注完成!\n共处理 %1 张图片\n检测到 %2 个目标").arg(m_imageFiles.size()).arg(totalDetections));
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

    QProgressDialog progress("正在自动标注...", "取消", 0, unannotated.size(), this);
    progress.setWindowModality(Qt::WindowModal);

    int totalDetections = 0;
    for (int i = 0; i < unannotated.size(); ++i) {
        if (progress.wasCanceled()) break;
        progress.setValue(i);
        progress.setLabelText(QString("正在处理: %1\n(%2/%3)").arg(unannotated[i]).arg(i+1).arg(unannotated.size()));
        QApplication::processEvents();

        QString imagePath = m_currentFolder + "/" + unannotated[i];
        QImage image(imagePath);
        if (image.isNull()) continue;

        auto annotations = m_detector->detect(image);
        totalDetections += annotations.size();

        // 保存
        QString txtPath = getAnnotationPath(imagePath);
        m_annotationIO.save(txtPath, annotations);
    }
    progress.setValue(unannotated.size());

    updateFileList();
    if (m_currentIndex >= 0) loadImage(m_currentIndex);

    QMessageBox::information(this, "完成",
        QString("标注完成!\n共处理 %1 张图片\n检测到 %2 个目标").arg(unannotated.size()).arg(totalDetections));
}
