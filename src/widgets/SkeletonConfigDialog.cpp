#include "SkeletonConfigDialog.h"
#include "io/DatasetConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QHeaderView>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QDialogButtonBox>
#include <cmath>

// DraggablePoint 实现
DraggablePoint::DraggablePoint(int index, SkeletonConfigDialog* dialog, qreal x, qreal y, qreal w, qreal h)
    : QGraphicsEllipseItem(x, y, w, h)
    , m_index(index)
    , m_dialog(dialog)
{
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setCursor(Qt::SizeAllCursor);
}

void DraggablePoint::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsEllipseItem::mouseMoveEvent(event);
    QPointF center = pos() + rect().center();
    m_dialog->onPointMoved(m_index, center);
}

void DraggablePoint::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsEllipseItem::mouseReleaseEvent(event);
    QPointF center = pos() + rect().center();
    m_dialog->onPointMoved(m_index, center);
}

// SkeletonConfigDialog 实现
SkeletonConfigDialog::SkeletonConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("骨架配置");
    setMinimumSize(750, 600);
    setupUI();
    updatePreview();
}

void SkeletonConfigDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 预设模板区域
    QGroupBox* presetGroup = new QGroupBox("预设模板");
    QHBoxLayout* presetLayout = new QHBoxLayout(presetGroup);
    m_presetCombo = new QComboBox();
    m_presetCombo->addItem("自定义");
    m_presetCombo->addItem("COCO 17 关键点");
    m_loadPresetBtn = new QPushButton("加载");
    m_saveBtn = new QPushButton("保存到项目");
    m_saveBtn->setToolTip("保存骨架配置到项目 YAML 文件");
    presetLayout->addWidget(m_presetCombo);
    presetLayout->addWidget(m_loadPresetBtn);
    presetLayout->addStretch();
    presetLayout->addWidget(m_saveBtn);
    mainLayout->addWidget(presetGroup);

    // 主编辑区域
    QHBoxLayout* editLayout = new QHBoxLayout();

    // 左侧：关键点和骨架连接
    QVBoxLayout* leftLayout = new QVBoxLayout();

    // 关键点表格
    QGroupBox* keypointGroup = new QGroupBox("关键点定义");
    QVBoxLayout* keypointLayout = new QVBoxLayout(keypointGroup);
    m_keypointTable = new QTableWidget(0, 3);
    m_keypointTable->setHorizontalHeaderLabels({"#", "名称", "颜色"});
    m_keypointTable->horizontalHeader()->setStretchLastSection(true);
    m_keypointTable->setColumnWidth(0, 30);
    m_keypointTable->setColumnWidth(1, 120);
    m_keypointTable->setColumnWidth(2, 50);
    m_keypointTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_keypointTable->setMaximumHeight(180);
    keypointLayout->addWidget(m_keypointTable);

    QHBoxLayout* keypointBtnLayout = new QHBoxLayout();
    m_addKeypointBtn = new QPushButton("+添加");
    m_removeKeypointBtn = new QPushButton("-删除");
    keypointBtnLayout->addWidget(m_addKeypointBtn);
    keypointBtnLayout->addWidget(m_removeKeypointBtn);
    keypointBtnLayout->addStretch();
    keypointLayout->addLayout(keypointBtnLayout);

    leftLayout->addWidget(keypointGroup);

    // 骨架连接表格
    QGroupBox* boneGroup = new QGroupBox("骨架连接");
    QVBoxLayout* boneLayout = new QVBoxLayout(boneGroup);
    m_boneTable = new QTableWidget(0, 3);
    m_boneTable->setHorizontalHeaderLabels({"从", "到", "颜色"});
    m_boneTable->horizontalHeader()->setStretchLastSection(true);
    m_boneTable->setColumnWidth(0, 80);
    m_boneTable->setColumnWidth(1, 80);
    m_boneTable->setColumnWidth(2, 50);
    m_boneTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_boneTable->setMaximumHeight(180);
    boneLayout->addWidget(m_boneTable);

    QHBoxLayout* boneBtnLayout = new QHBoxLayout();
    m_addBoneBtn = new QPushButton("+添加");
    m_removeBoneBtn = new QPushButton("-删除");
    boneBtnLayout->addWidget(m_addBoneBtn);
    boneBtnLayout->addWidget(m_removeBoneBtn);
    boneBtnLayout->addStretch();
    boneLayout->addLayout(boneBtnLayout);

    leftLayout->addWidget(boneGroup);
    editLayout->addLayout(leftLayout);

    // 右侧：预览面板
    QGroupBox* previewGroup = new QGroupBox("预览 (可拖动关键点)");
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    m_previewScene = new QGraphicsScene(this);
    m_previewView = new QGraphicsView(m_previewScene);
    m_previewView->setMinimumSize(280, 350);
    m_previewView->setRenderHint(QPainter::Antialiasing);
    m_previewView->setBackgroundBrush(QColor(50, 50, 50));
    m_previewView->setDragMode(QGraphicsView::NoDrag);
    previewLayout->addWidget(m_previewView);
    editLayout->addWidget(previewGroup);

    mainLayout->addLayout(editLayout);

    // 按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);

    // 连接信号
    connect(m_loadPresetBtn, &QPushButton::clicked, this, &SkeletonConfigDialog::onLoadPreset);
    connect(m_saveBtn, &QPushButton::clicked, this, &SkeletonConfigDialog::onSaveToProject);
    connect(m_addKeypointBtn, &QPushButton::clicked, this, &SkeletonConfigDialog::onAddKeypoint);
    connect(m_removeKeypointBtn, &QPushButton::clicked, this, &SkeletonConfigDialog::onRemoveKeypoint);
    connect(m_addBoneBtn, &QPushButton::clicked, this, &SkeletonConfigDialog::onAddBone);
    connect(m_removeBoneBtn, &QPushButton::clicked, this, &SkeletonConfigDialog::onRemoveBone);
    connect(m_keypointTable, &QTableWidget::cellChanged, this, &SkeletonConfigDialog::onKeypointCellChanged);
    connect(m_keypointTable, &QTableWidget::cellClicked, [this](int row, int column) {
        if (column == 2 && row < m_config.keypointCount()) {
            QColor color = QColorDialog::getColor(m_config.keypointColor(row), this, "选择关键点颜色");
            if (color.isValid()) {
                KeypointDef def = m_config.keypointDefs()[row];
                def.color = color;
                m_config.setKeypointDef(row, def);
                refreshKeypointTable();
                updatePreview();
            }
        }
    });
    connect(m_boneTable, &QTableWidget::cellClicked, [this](int row, int column) {
        if (column == 2 && row < m_config.bones().size()) {
            QColor color = QColorDialog::getColor(m_config.bones()[row].color, this, "选择连线颜色");
            if (color.isValid()) {
                auto bones = m_config.bones();
                bones[row].color = color;
                m_config.clearBones();
                for (const auto& bone : bones) {
                    m_config.addBone(bone.from, bone.to, bone.color);
                }
                refreshBoneTable();
                updatePreviewLines();
            }
        }
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SkeletonConfigDialog::setConfig(const SkeletonConfig& config)
{
    m_config = config;
    m_previewPositions.clear();
    refreshKeypointTable();
    refreshBoneTable();
    updatePreview();
}

void SkeletonConfigDialog::onLoadPreset()
{
    int index = m_presetCombo->currentIndex();
    if (index == 0) {
        // 自定义 - 从项目加载
        if (m_projectPath.isEmpty() || !QFile::exists(m_projectPath)) {
            QMessageBox::information(this, "提示", "未找到项目文件，请先打开项目");
            return;
        }
        DatasetConfig projectConfig;
        if (projectConfig.loadYAML(m_projectPath)) {
            m_config = projectConfig.skeletonConfig();
        } else {
            QMessageBox::warning(this, "错误", "无法读取项目配置");
            return;
        }
    } else if (index == 1) {
        // COCO 17
        m_config = SkeletonConfig::createCOCO17();
    }
    m_previewPositions.clear();
    refreshKeypointTable();
    refreshBoneTable();
    updatePreview();
}

void SkeletonConfigDialog::onSaveToProject()
{
    if (m_projectPath.isEmpty()) {
        // 没有项目路径，让用户选择保存位置
        QString path = QFileDialog::getSaveFileName(this, "保存骨架配置", QString(), "YAML 文件 (*.yaml *.yml)");
        if (path.isEmpty()) return;
        m_projectPath = path;
    }

    // 加载现有配置（如果存在）
    DatasetConfig projectConfig;
    if (QFile::exists(m_projectPath)) {
        projectConfig.loadYAML(m_projectPath);
    }

    // 更新骨架配置
    projectConfig.setSkeletonConfig(m_config);
    projectConfig.setKptShape(m_config.keypointCount(), 3);
    if (m_config.keypointCount() > 0) {
        projectConfig.setTaskType(DatasetConfig::Pose);
    }

    // 保存
    if (projectConfig.saveYAML(m_projectPath)) {
        QMessageBox::information(this, "成功", "骨架配置已保存到:\n" + m_projectPath);
    } else {
        QMessageBox::warning(this, "错误", "无法保存到:\n" + m_projectPath);
    }
}

void SkeletonConfigDialog::onAddKeypoint()
{
    int count = m_config.keypointCount();
    m_config.setKeypointCount(count + 1);
    KeypointDef def(QString("keypoint_%1").arg(count), Qt::red);
    m_config.setKeypointDef(count, def);

    if (m_previewPositions.size() < count + 1) {
        auto defaults = getDefaultPositions(count + 1);
        if (count < defaults.size()) {
            m_previewPositions.append(defaults[count]);
        } else {
            m_previewPositions.append(QPointF(100 + count * 10, 100));
        }
    }

    refreshKeypointTable();
    refreshBoneTable();
    updatePreview();
}

void SkeletonConfigDialog::onRemoveKeypoint()
{
    int row = m_keypointTable->currentRow();
    if (row < 0 || row >= m_config.keypointCount()) return;

    int count = m_config.keypointCount();
    QVector<KeypointDef> defs;
    for (int i = 0; i < count; ++i) {
        if (i != row) {
            defs.append(m_config.keypointDefs()[i]);
        }
    }

    QVector<BoneConnection> bones;
    for (const auto& bone : m_config.bones()) {
        if (bone.from != row && bone.to != row) {
            int newFrom = bone.from > row ? bone.from - 1 : bone.from;
            int newTo = bone.to > row ? bone.to - 1 : bone.to;
            bones.append(BoneConnection(newFrom, newTo, bone.color));
        }
    }

    if (row < m_previewPositions.size()) {
        m_previewPositions.remove(row);
    }

    m_config.setKeypointCount(defs.size());
    for (int i = 0; i < defs.size(); ++i) {
        m_config.setKeypointDef(i, defs[i]);
    }
    m_config.clearBones();
    for (const auto& bone : bones) {
        m_config.addBone(bone.from, bone.to, bone.color);
    }

    refreshKeypointTable();
    refreshBoneTable();
    updatePreview();
}

void SkeletonConfigDialog::onAddBone()
{
    if (m_config.keypointCount() < 2) return;
    m_config.addBone(0, 1, Qt::cyan);
    refreshBoneTable();
    updatePreviewLines();
}

void SkeletonConfigDialog::onRemoveBone()
{
    int row = m_boneTable->currentRow();
    if (row >= 0 && row < m_config.bones().size()) {
        m_config.removeBone(row);
        refreshBoneTable();
        updatePreviewLines();
    }
}

void SkeletonConfigDialog::onKeypointCellChanged(int row, int column)
{
    if (row >= m_config.keypointCount()) return;
    if (column == 1) {
        QTableWidgetItem* item = m_keypointTable->item(row, column);
        if (item) {
            KeypointDef def = m_config.keypointDefs()[row];
            def.name = item->text();
            m_config.setKeypointDef(row, def);
            updateBoneComboBoxes();
        }
    }
}

void SkeletonConfigDialog::onBoneCellChanged(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);
}

void SkeletonConfigDialog::onPointMoved(int index, const QPointF& newPos)
{
    if (index >= 0 && index < m_previewPositions.size()) {
        m_previewPositions[index] = newPos;
        updatePreviewLines();
        updatePreviewLabels();
    }
}

void SkeletonConfigDialog::refreshKeypointTable()
{
    m_keypointTable->blockSignals(true);
    m_keypointTable->setRowCount(m_config.keypointCount());

    for (int i = 0; i < m_config.keypointCount(); ++i) {
        const KeypointDef& def = m_config.keypointDefs()[i];

        QTableWidgetItem* indexItem = new QTableWidgetItem(QString::number(i));
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        m_keypointTable->setItem(i, 0, indexItem);

        QTableWidgetItem* nameItem = new QTableWidgetItem(def.name);
        m_keypointTable->setItem(i, 1, nameItem);

        QTableWidgetItem* colorItem = new QTableWidgetItem();
        colorItem->setBackground(def.color);
        colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
        m_keypointTable->setItem(i, 2, colorItem);
    }
    m_keypointTable->blockSignals(false);
}

void SkeletonConfigDialog::refreshBoneTable()
{
    m_suppressBoneSignals = true;  // 防止 setCurrentIndex 触发 clearBones 风暴
    m_boneTable->blockSignals(true);
    m_boneTable->clearContents();
    const auto& bones = m_config.bones();
    m_boneTable->setRowCount(bones.size());

    for (int i = 0; i < bones.size(); ++i) {
        const BoneConnection& bone = bones[i];
        const int boneIndex = i;

        QComboBox* fromCombo = new QComboBox();
        for (int j = 0; j < m_config.keypointCount(); ++j) {
            fromCombo->addItem(QString("%1: %2").arg(j).arg(m_config.keypointName(j)), j);
        }
        fromCombo->setCurrentIndex(bone.from);
        connect(fromCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, boneIndex](int newFrom) {
            if (m_suppressBoneSignals) return;
            auto bones = m_config.bones();
            if (boneIndex < bones.size()) {
                BoneConnection newBone(newFrom, bones[boneIndex].to, bones[boneIndex].color);
                m_config.clearBones();
                for (int j = 0; j < bones.size(); ++j) {
                    if (j == boneIndex)
                        m_config.addBone(newBone.from, newBone.to, newBone.color);
                    else
                        m_config.addBone(bones[j].from, bones[j].to, bones[j].color);
                }
                updatePreviewLines();
            }
        });
        m_boneTable->setCellWidget(i, 0, fromCombo);

        QComboBox* toCombo = new QComboBox();
        for (int j = 0; j < m_config.keypointCount(); ++j) {
            toCombo->addItem(QString("%1: %2").arg(j).arg(m_config.keypointName(j)), j);
        }
        toCombo->setCurrentIndex(bone.to);
        connect(toCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, boneIndex](int newTo) {
            if (m_suppressBoneSignals) return;
            auto bones = m_config.bones();
            if (boneIndex < bones.size()) {
                BoneConnection newBone(bones[boneIndex].from, newTo, bones[boneIndex].color);
                m_config.clearBones();
                for (int j = 0; j < bones.size(); ++j) {
                    if (j == boneIndex)
                        m_config.addBone(newBone.from, newBone.to, newBone.color);
                    else
                        m_config.addBone(bones[j].from, bones[j].to, bones[j].color);
                }
                updatePreviewLines();
            }
        });
        m_boneTable->setCellWidget(i, 1, toCombo);

        QTableWidgetItem* colorItem = new QTableWidgetItem();
        colorItem->setBackground(bone.color);
        colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
        m_boneTable->setItem(i, 2, colorItem);
    }
    m_boneTable->blockSignals(false);
    m_suppressBoneSignals = false;  // 恢复信号处理
}

void SkeletonConfigDialog::updateBoneComboBoxes()
{
    refreshBoneTable();
}

void SkeletonConfigDialog::updatePreview()
{
    m_previewScene->clear();
    m_previewPoints.clear();
    m_previewLines.clear();
    m_previewLabels.clear();

    int count = m_config.keypointCount();
    if (count == 0) return;

    if (m_previewPositions.size() != count) {
        m_previewPositions = getDefaultPositions(count);
    }

    // 绘制可拖动的关键点和标签
    for (int i = 0; i < count; ++i) {
        const KeypointDef& def = m_config.keypointDefs()[i];
        const QPointF& pos = m_previewPositions[i];

        DraggablePoint* point = new DraggablePoint(i, this, -6, -6, 12, 12);
        point->setPos(pos);
        point->setPen(QPen(Qt::black, 1));
        point->setBrush(QBrush(def.color));
        point->setZValue(1);
        m_previewScene->addItem(point);
        m_previewPoints.append(point);

        // 索引标签
        QGraphicsTextItem* label = m_previewScene->addText(QString::number(i));
        label->setDefaultTextColor(Qt::white);
        label->setPos(pos.x() + 8, pos.y() - 10);
        label->setScale(0.9);
        label->setZValue(2);
        m_previewLabels.append(label);
    }

    // 绘制骨架连线
    updatePreviewLines();

    m_previewView->fitInView(m_previewScene->sceneRect().adjusted(-20, -20, 20, 20), Qt::KeepAspectRatio);
}

void SkeletonConfigDialog::updatePreviewLines()
{
    // 确保预览位置与关键点数量同步
    int count = m_config.keypointCount();
    if (m_previewPositions.size() != count) {
        m_previewPositions = getDefaultPositions(count);
    }

    const auto& bones = m_config.bones();

    // 如果数量不匹配，完全重建（处理添加/删除骨骼）
    if (m_previewLines.size() != bones.size()) {
        for (auto* line : m_previewLines) {
            m_previewScene->removeItem(line);
            delete line;
        }
        m_previewLines.clear();

        for (const auto& bone : bones) {
            if (bone.from >= 0 && bone.from < m_previewPositions.size() &&
                bone.to >= 0 && bone.to < m_previewPositions.size()) {
                QGraphicsLineItem* line = m_previewScene->addLine(
                    m_previewPositions[bone.from].x(), m_previewPositions[bone.from].y(),
                    m_previewPositions[bone.to].x(), m_previewPositions[bone.to].y(),
                    QPen(bone.color, 2));
                line->setZValue(0);
                m_previewLines.append(line);
            }
        }
    } else {
        // 数量匹配时原地更新位置和颜色
        for (int i = 0; i < bones.size() && i < m_previewLines.size(); ++i) {
            const auto& bone = bones[i];
            if (bone.from >= 0 && bone.from < m_previewPositions.size() &&
                bone.to >= 0 && bone.to < m_previewPositions.size()) {
                m_previewLines[i]->setLine(
                    m_previewPositions[bone.from].x(), m_previewPositions[bone.from].y(),
                    m_previewPositions[bone.to].x(), m_previewPositions[bone.to].y());
                m_previewLines[i]->setPen(QPen(bone.color, 2));
            }
        }
    }
}

void SkeletonConfigDialog::updatePreviewLabels()
{
    // 更新标签位置跟随关键点
    for (int i = 0; i < m_previewLabels.size() && i < m_previewPositions.size(); ++i) {
        const QPointF& pos = m_previewPositions[i];
        m_previewLabels[i]->setPos(pos.x() + 8, pos.y() - 10);
    }
}

QVector<QPointF> SkeletonConfigDialog::getDefaultPositions(int count) const
{
    QVector<QPointF> positions;

    if (count == 17) {
        positions = {
            {100, 20},   // 0: nose
            {90, 10},    // 1: left_eye
            {110, 10},   // 2: right_eye
            {75, 20},    // 3: left_ear
            {125, 20},   // 4: right_ear
            {70, 60},    // 5: left_shoulder
            {130, 60},   // 6: right_shoulder
            {50, 100},   // 7: left_elbow
            {150, 100},  // 8: right_elbow
            {35, 140},   // 9: left_wrist
            {165, 140},  // 10: right_wrist
            {80, 130},   // 11: left_hip
            {120, 130},  // 12: right_hip
            {75, 180},   // 13: left_knee
            {125, 180},  // 14: right_knee
            {70, 230},   // 15: left_ankle
            {130, 230},  // 16: right_ankle
        };
    } else {
        double centerX = 100;
        double centerY = 120;
        double radius = 80;

        for (int i = 0; i < count; ++i) {
            double angle = -M_PI / 2 + (2 * M_PI * i / count);
            double x = centerX + radius * std::cos(angle);
            double y = centerY + radius * std::sin(angle);
            positions.append(QPointF(x, y));
        }
    }

    return positions;
}
