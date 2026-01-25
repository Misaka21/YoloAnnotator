#include "NewProjectDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>

NewProjectDialog::NewProjectDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("创建新项目");
    setMinimumWidth(500);
    setupUI();
}

void NewProjectDialog::setClassNames(const QStringList& names) {
    m_classNames = names;
    m_classesInfoLabel->setText(QString("已加载 %1 个类别").arg(names.size()));
}

void NewProjectDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 图片路径组
    QGroupBox* imageGroup = new QGroupBox("图片路径");
    QHBoxLayout* imageLayout = new QHBoxLayout(imageGroup);

    m_imagePathEdit = new QLineEdit();
    m_imagePathEdit->setPlaceholderText("选择包含图片的文件夹...");
    m_browseImagesBtn = new QPushButton("浏览...");

    imageLayout->addWidget(m_imagePathEdit);
    imageLayout->addWidget(m_browseImagesBtn);
    mainLayout->addWidget(imageGroup);

    // 任务类型组
    QGroupBox* taskGroup = new QGroupBox("任务类型");
    QVBoxLayout* taskLayout = new QVBoxLayout(taskGroup);

    m_detectRadio = new QRadioButton("目标检测 (Detection)");
    m_poseRadio = new QRadioButton("姿态估计 (Pose)");
    m_detectRadio->setChecked(true);

    m_taskGroup = new QButtonGroup(this);
    m_taskGroup->addButton(m_detectRadio, 0);
    m_taskGroup->addButton(m_poseRadio, 1);

    taskLayout->addWidget(m_detectRadio);
    taskLayout->addWidget(m_poseRadio);

    // 关键点数量（仅 Pose 模式）
    QHBoxLayout* kpLayout = new QHBoxLayout();
    m_keypointLabel = new QLabel("关键点数量:");
    m_keypointSpinBox = new QSpinBox();
    m_keypointSpinBox->setRange(1, 50);
    m_keypointSpinBox->setValue(17);  // COCO 默认
    m_keypointSpinBox->setEnabled(false);
    m_keypointLabel->setEnabled(false);

    kpLayout->addWidget(m_keypointLabel);
    kpLayout->addWidget(m_keypointSpinBox);
    kpLayout->addStretch();
    taskLayout->addLayout(kpLayout);

    // 关键点维度（仅 Pose 模式）
    QHBoxLayout* dimLayout = new QHBoxLayout();
    m_kptDimLabel = new QLabel("关键点格式:");
    m_kptDimCombo = new QComboBox();
    m_kptDimCombo->addItem("x, y, visibility (标准格式)", 3);
    m_kptDimCombo->addItem("x, y (无可见性)", 2);
    m_kptDimCombo->setEnabled(false);
    m_kptDimLabel->setEnabled(false);

    dimLayout->addWidget(m_kptDimLabel);
    dimLayout->addWidget(m_kptDimCombo);
    dimLayout->addStretch();
    taskLayout->addLayout(dimLayout);

    mainLayout->addWidget(taskGroup);

    // 类别信息
    m_classesInfoLabel = new QLabel("未加载类别");
    m_classesInfoLabel->setStyleSheet("color: gray;");
    mainLayout->addWidget(m_classesInfoLabel);

    // 保存路径组
    QGroupBox* saveGroup = new QGroupBox("项目保存位置");
    QHBoxLayout* saveLayout = new QHBoxLayout(saveGroup);

    m_savePathEdit = new QLineEdit();
    m_savePathEdit->setPlaceholderText("保存 dataset.yaml 的路径...");
    m_browseSaveBtn = new QPushButton("浏览...");

    saveLayout->addWidget(m_savePathEdit);
    saveLayout->addWidget(m_browseSaveBtn);
    mainLayout->addWidget(saveGroup);

    // 按钮
    mainLayout->addStretch();

    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_createBtn = new QPushButton("创建项目");
    m_cancelBtn = new QPushButton("取消");
    m_createBtn->setDefault(true);

    btnLayout->addStretch();
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_createBtn);
    mainLayout->addLayout(btnLayout);

    // 连接信号
    connect(m_browseImagesBtn, &QPushButton::clicked, this, &NewProjectDialog::onBrowseImages);
    connect(m_browseSaveBtn, &QPushButton::clicked, this, &NewProjectDialog::onBrowseSavePath);
    connect(m_taskGroup, &QButtonGroup::idToggled, this, &NewProjectDialog::onTaskTypeChanged);
    connect(m_createBtn, &QPushButton::clicked, this, &NewProjectDialog::onAccept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void NewProjectDialog::onBrowseImages() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择图片文件夹",
        m_imagePathEdit->text().isEmpty() ? QString() : m_imagePathEdit->text());

    if (!dir.isEmpty()) {
        m_imagePathEdit->setText(dir);

        // 自动设置保存路径
        if (m_savePathEdit->text().isEmpty()) {
            m_savePathEdit->setText(dir + "/dataset.yaml");
        }
    }
}

void NewProjectDialog::onBrowseSavePath() {
    QString defaultPath = m_savePathEdit->text();
    if (defaultPath.isEmpty() && !m_imagePathEdit->text().isEmpty()) {
        defaultPath = m_imagePathEdit->text() + "/dataset.yaml";
    }

    QString path = QFileDialog::getSaveFileName(this, "保存项目文件",
        defaultPath, "YAML 文件 (*.yaml *.yml)");

    if (!path.isEmpty()) {
        // 确保扩展名
        if (!path.endsWith(".yaml") && !path.endsWith(".yml")) {
            path += ".yaml";
        }
        m_savePathEdit->setText(path);
    }
}

void NewProjectDialog::onTaskTypeChanged() {
    bool isPose = m_poseRadio->isChecked();
    m_keypointLabel->setEnabled(isPose);
    m_keypointSpinBox->setEnabled(isPose);
    m_kptDimLabel->setEnabled(isPose);
    m_kptDimCombo->setEnabled(isPose);
}

void NewProjectDialog::onAccept() {
    // 验证输入
    if (m_imagePathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "错误", "请选择图片文件夹");
        return;
    }

    QFileInfo imgDir(m_imagePathEdit->text());
    if (!imgDir.exists() || !imgDir.isDir()) {
        QMessageBox::warning(this, "错误", "图片文件夹不存在");
        return;
    }

    if (m_savePathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "错误", "请指定项目保存路径");
        return;
    }

    if (m_classNames.isEmpty()) {
        QMessageBox::warning(this, "错误", "没有加载类别信息");
        return;
    }

    // 构建配置
    m_config.clear();
    m_config.setClassNames(m_classNames);

    // 设置路径
    QFileInfo saveInfo(m_savePathEdit->text());
    QString datasetRoot = saveInfo.absolutePath();
    m_config.setDatasetPath(datasetRoot);

    // 计算图片路径相对于数据集根目录的相对路径
    QString imgPath = m_imagePathEdit->text();
    QDir rootDir(datasetRoot);
    QString relativePath = rootDir.relativeFilePath(imgPath);

    // 默认设置为 train 路径
    m_config.setTrainPath(relativePath);

    // 任务类型
    if (m_poseRadio->isChecked()) {
        m_config.setTaskType(DatasetConfig::Pose);
        int kpCount = m_keypointSpinBox->value();
        int kpDim = m_kptDimCombo->currentData().toInt();
        m_config.setKptShape(kpCount, kpDim);
        m_config.setSkeletonConfig(SkeletonConfig::createCustom(kpCount));
    } else {
        m_config.setTaskType(DatasetConfig::Detection);
    }

    accept();
}
