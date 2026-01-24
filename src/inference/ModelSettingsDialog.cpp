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

    settingsLayout->addRow("置信度阈值:", m_confSpinBox);
    settingsLayout->addRow("IoU 阈值:", m_iouSpinBox);

    mainLayout->addWidget(settingsGroup);

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
        QMessageBox::information(this, "成功", "模型加载成功");
    } else {
        QMessageBox::critical(this, "错误", "模型加载失败");
    }
}

void ModelSettingsDialog::onApplySettings()
{
    m_detector->setConfThreshold(m_confSpinBox->value());
    m_detector->setIouThreshold(m_iouSpinBox->value());
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
