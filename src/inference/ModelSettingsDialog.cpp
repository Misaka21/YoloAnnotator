#include "ModelSettingsDialog.h"
#include "YoloDetector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>

ModelSettingsDialog::ModelSettingsDialog(YoloDetector* detector, QWidget* parent)
    : QDialog(parent)
    , m_detector(detector)
{
    setWindowTitle("模型设置");
    setMinimumWidth(450);
    setupUI();
    updateModelInfo();
}

void ModelSettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Model selection group
    QGroupBox* modelGroup = new QGroupBox("ONNX 模型");
    QVBoxLayout* modelLayout = new QVBoxLayout(modelGroup);

    QHBoxLayout* pathLayout = new QHBoxLayout();
    m_modelPathEdit = new QLineEdit();
    m_modelPathEdit->setPlaceholderText("选择 ONNX 模型文件...");
    if (m_detector->isLoaded()) {
        m_modelPathEdit->setText(m_detector->modelPath());
    }
    m_browseBtn = new QPushButton("浏览...");
    m_loadBtn = new QPushButton("加载");

    pathLayout->addWidget(m_modelPathEdit);
    pathLayout->addWidget(m_browseBtn);
    pathLayout->addWidget(m_loadBtn);
    modelLayout->addLayout(pathLayout);

    // Model info
    QFormLayout* infoLayout = new QFormLayout();
    m_modelTypeLabel = new QLabel("-");
    m_inputSizeLabel = new QLabel("-");
    m_classesLabel = new QLabel("-");
    infoLayout->addRow("模型类型:", m_modelTypeLabel);
    infoLayout->addRow("输入尺寸:", m_inputSizeLabel);
    infoLayout->addRow("类别/关键点:", m_classesLabel);
    modelLayout->addLayout(infoLayout);

    mainLayout->addWidget(modelGroup);

    // Inference settings group
    QGroupBox* settingsGroup = new QGroupBox("推理参数");
    QFormLayout* settingsLayout = new QFormLayout(settingsGroup);

    m_confSpinBox = new QDoubleSpinBox();
    m_confSpinBox->setRange(0.01, 1.0);
    m_confSpinBox->setSingleStep(0.05);
    m_confSpinBox->setValue(m_detector->confThreshold());
    m_confSpinBox->setDecimals(2);

    m_iouSpinBox = new QDoubleSpinBox();
    m_iouSpinBox->setRange(0.01, 1.0);
    m_iouSpinBox->setSingleStep(0.05);
    m_iouSpinBox->setValue(m_detector->iouThreshold());
    m_iouSpinBox->setDecimals(2);

    m_backendCombo = new QComboBox();

    // 只显示运行时可用的后端
    QVector<InferenceBackend> backends = YoloDetector::availableBackends();
    for (InferenceBackend backend : backends) {
        m_backendCombo->addItem(YoloDetector::backendName(backend), static_cast<int>(backend));
    }

    // 设置当前后端
    int currentBackend = static_cast<int>(m_detector->backend());
    for (int i = 0; i < m_backendCombo->count(); i++) {
        if (m_backendCombo->itemData(i).toInt() == currentBackend) {
            m_backendCombo->setCurrentIndex(i);
            break;
        }
    }

    settingsLayout->addRow("置信度阈值:", m_confSpinBox);
    settingsLayout->addRow("IoU 阈值:", m_iouSpinBox);
    settingsLayout->addRow("推理后端:", m_backendCombo);

    mainLayout->addWidget(settingsGroup);

    // Class mapping group
    m_mappingGroup = new QGroupBox("类别映射");
    QVBoxLayout* mappingGroupLayout = new QVBoxLayout(m_mappingGroup);

    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumHeight(120);
    scrollArea->setMaximumHeight(200);

    m_mappingContainer = new QWidget();
    m_mappingLayout = new QVBoxLayout(m_mappingContainer);
    m_mappingLayout->setContentsMargins(4, 4, 4, 4);
    m_mappingLayout->setSpacing(4);

    QLabel* placeholderLabel = new QLabel("加载模型后显示类别映射");
    placeholderLabel->setStyleSheet("color: gray;");
    placeholderLabel->setAlignment(Qt::AlignCenter);
    m_mappingLayout->addWidget(placeholderLabel);
    m_mappingLayout->addStretch();

    scrollArea->setWidget(m_mappingContainer);
    mappingGroupLayout->addWidget(scrollArea);

    mainLayout->addWidget(m_mappingGroup);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* applyBtn = new QPushButton("应用");
    QPushButton* closeBtn = new QPushButton("关闭");
    buttonLayout->addStretch();
    buttonLayout->addWidget(applyBtn);
    buttonLayout->addWidget(closeBtn);
    mainLayout->addLayout(buttonLayout);

    // Connections
    connect(m_browseBtn, &QPushButton::clicked, this, &ModelSettingsDialog::onBrowseModel);
    connect(m_loadBtn, &QPushButton::clicked, this, &ModelSettingsDialog::onLoadModel);
    connect(applyBtn, &QPushButton::clicked, this, &ModelSettingsDialog::onApplySettings);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void ModelSettingsDialog::onBrowseModel()
{
    QString filter = "ONNX Models (*.onnx);;All Files (*)";
    QString path = QFileDialog::getOpenFileName(this, "选择 ONNX 模型", QString(), filter);
    if (!path.isEmpty()) {
        m_modelPathEdit->setText(path);
    }
}

void ModelSettingsDialog::onLoadModel()
{
    QString path = m_modelPathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先选择模型文件");
        return;
    }

    setCursor(Qt::WaitCursor);
    bool success = m_detector->loadModel(path);
    setCursor(Qt::ArrowCursor);

    if (success) {
        updateModelInfo();
        rebuildMappingUI();
        QMessageBox::information(this, "成功", "模型加载成功");
    } else {
        QMessageBox::critical(this, "错误", "模型加载失败");
    }
}

void ModelSettingsDialog::onApplySettings()
{
    if (!m_detector->isLoaded()) {
        QMessageBox::warning(this, "警告", "请先加载模型");
        return;
    }

    m_detector->setConfThreshold(m_confSpinBox->value());
    m_detector->setIouThreshold(m_iouSpinBox->value());

    // 使用 itemData 获取正确的后端枚举值
    int backendValue = m_backendCombo->currentData().toInt();
    m_detector->setBackend(static_cast<InferenceBackend>(backendValue));

    // 收集类别映射配置
    QVector<ClassMapping> mappings;
    for (int i = 0; i < m_enableChecks.size(); ++i) {
        ClassMapping mapping;
        mapping.enabled = m_enableChecks[i]->isChecked();
        mapping.targetClassId = m_targetCombos[i]->currentIndex() - 1;  // -1 因为第一项是"保持原ID"
        mappings.append(mapping);
    }
    m_detector->setClassMappings(mappings);

    QMessageBox::information(this, "提示", "参数已应用");
}

void ModelSettingsDialog::updateModelInfo()
{
    if (m_detector->isLoaded()) {
        m_modelTypeLabel->setText(m_detector->modelTypeString());
        m_inputSizeLabel->setText(QString("%1 x %2")
            .arg(m_detector->inputSize().width())
            .arg(m_detector->inputSize().height()));

        if (m_detector->modelType() == ModelType::Detection) {
            m_classesLabel->setText(QString("%1 类别").arg(m_detector->numClasses()));
        } else if (m_detector->modelType() == ModelType::Pose) {
            m_classesLabel->setText(QString("%1 关键点").arg(m_detector->numKeypoints()));
        } else {
            m_classesLabel->setText("-");
        }
    } else {
        m_modelTypeLabel->setText("-");
        m_inputSizeLabel->setText("-");
        m_classesLabel->setText("-");
    }
}

void ModelSettingsDialog::setDatasetClasses(const QStringList& classes)
{
    m_datasetClasses = classes;
    // 如果模型已加载，重建映射 UI
    if (m_detector->isLoaded()) {
        rebuildMappingUI();
    }
}

void ModelSettingsDialog::rebuildMappingUI()
{
    // 清空现有的映射控件
    m_enableChecks.clear();
    m_targetCombos.clear();

    // 删除所有子控件
    QLayoutItem* item;
    while ((item = m_mappingLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    if (!m_detector->isLoaded()) {
        QLabel* label = new QLabel("加载模型后显示类别映射");
        label->setStyleSheet("color: gray;");
        label->setAlignment(Qt::AlignCenter);
        m_mappingLayout->addWidget(label);
        m_mappingLayout->addStretch();
        return;
    }

    int numClasses = m_detector->numClasses();

    // 获取已有的映射配置
    QVector<ClassMapping> existingMappings = m_detector->classMappings();

    for (int i = 0; i < numClasses; ++i) {
        QHBoxLayout* rowLayout = new QHBoxLayout();
        rowLayout->setSpacing(8);

        // 启用复选框
        QCheckBox* enableCheck = new QCheckBox(QString("class_%1").arg(i));
        enableCheck->setChecked(true);
        enableCheck->setMinimumWidth(80);

        // 箭头标签
        QLabel* arrowLabel = new QLabel("→");

        // 目标类别下拉框
        QComboBox* targetCombo = new QComboBox();
        targetCombo->addItem("保持原ID");  // 索引 0
        for (int j = 0; j < m_datasetClasses.size(); ++j) {
            targetCombo->addItem(QString("%1: %2").arg(j).arg(m_datasetClasses[j]));
        }
        targetCombo->setMinimumWidth(150);

        // 恢复之前的配置
        if (i < existingMappings.size()) {
            enableCheck->setChecked(existingMappings[i].enabled);
            int targetIdx = existingMappings[i].targetClassId;
            if (targetIdx >= 0 && targetIdx < m_datasetClasses.size()) {
                targetCombo->setCurrentIndex(targetIdx + 1);  // +1 因为第一项是"保持原ID"
            }
        } else {
            // 默认映射到同索引的数据集类别
            if (i < m_datasetClasses.size()) {
                targetCombo->setCurrentIndex(i + 1);
            }
        }

        rowLayout->addWidget(enableCheck);
        rowLayout->addWidget(arrowLabel);
        rowLayout->addWidget(targetCombo);
        rowLayout->addStretch();

        m_enableChecks.append(enableCheck);
        m_targetCombos.append(targetCombo);

        QWidget* rowWidget = new QWidget();
        rowWidget->setLayout(rowLayout);
        m_mappingLayout->addWidget(rowWidget);
    }

    m_mappingLayout->addStretch();
}
