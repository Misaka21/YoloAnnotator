#include "SplitDatasetDialog.h"
#include "io/DatasetConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRandomGenerator>
#include <QApplication>
#include <algorithm>

SplitDatasetDialog::SplitDatasetDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("分割数据集");
    setMinimumWidth(550);
    setupUI();
}

void SplitDatasetDialog::setSourcePath(const QString& imagesPath, const QString& labelsPath) {
    m_sourceImagesEdit->setText(imagesPath);
    m_sourceLabelsEdit->setText(labelsPath);

    // 自动设置输出路径为上级目录
    if (!imagesPath.isEmpty()) {
        QDir dir(imagesPath);
        dir.cdUp();
        if (dir.dirName() == "images") {
            dir.cdUp();
        }
        m_outputPathEdit->setText(dir.absolutePath() + "/split_dataset");
    }
}

void SplitDatasetDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // === 源路径组 ===
    QGroupBox* sourceGroup = new QGroupBox("源数据");
    QVBoxLayout* sourceLayout = new QVBoxLayout(sourceGroup);

    QHBoxLayout* imgLayout = new QHBoxLayout();
    imgLayout->addWidget(new QLabel("图片文件夹:"));
    m_sourceImagesEdit = new QLineEdit();
    m_sourceImagesEdit->setPlaceholderText("选择包含图片的文件夹...");
    imgLayout->addWidget(m_sourceImagesEdit);
    sourceLayout->addLayout(imgLayout);

    QHBoxLayout* lblLayout = new QHBoxLayout();
    lblLayout->addWidget(new QLabel("标签文件夹:"));
    m_sourceLabelsEdit = new QLineEdit();
    m_sourceLabelsEdit->setPlaceholderText("选择包含标签的文件夹（可选，自动检测）...");
    lblLayout->addWidget(m_sourceLabelsEdit);
    m_browseSourceBtn = new QPushButton("浏览...");
    lblLayout->addWidget(m_browseSourceBtn);
    sourceLayout->addLayout(lblLayout);

    mainLayout->addWidget(sourceGroup);

    // === 输出路径组 ===
    QGroupBox* outputGroup = new QGroupBox("输出位置");
    QHBoxLayout* outputLayout = new QHBoxLayout(outputGroup);
    m_outputPathEdit = new QLineEdit();
    m_outputPathEdit->setPlaceholderText("选择输出文件夹...");
    m_browseOutputBtn = new QPushButton("浏览...");
    outputLayout->addWidget(m_outputPathEdit);
    outputLayout->addWidget(m_browseOutputBtn);
    mainLayout->addWidget(outputGroup);

    // === 分割比例组 ===
    QGroupBox* ratioGroup = new QGroupBox("分割比例");
    QVBoxLayout* ratioLayout = new QVBoxLayout(ratioGroup);

    QHBoxLayout* ratioInputLayout = new QHBoxLayout();

    ratioInputLayout->addWidget(new QLabel("训练集:"));
    m_trainRatioSpin = new QSpinBox();
    m_trainRatioSpin->setRange(1, 100);
    m_trainRatioSpin->setValue(70);
    m_trainRatioSpin->setSuffix("%");
    ratioInputLayout->addWidget(m_trainRatioSpin);

    ratioInputLayout->addWidget(new QLabel("验证集:"));
    m_valRatioSpin = new QSpinBox();
    m_valRatioSpin->setRange(1, 100);
    m_valRatioSpin->setValue(20);
    m_valRatioSpin->setSuffix("%");
    ratioInputLayout->addWidget(m_valRatioSpin);

    m_testEnabledCheck = new QCheckBox("测试集:");
    m_testEnabledCheck->setChecked(true);
    ratioInputLayout->addWidget(m_testEnabledCheck);

    m_testRatioSpin = new QSpinBox();
    m_testRatioSpin->setRange(0, 100);
    m_testRatioSpin->setValue(10);
    m_testRatioSpin->setSuffix("%");
    ratioInputLayout->addWidget(m_testRatioSpin);

    ratioInputLayout->addStretch();
    ratioLayout->addLayout(ratioInputLayout);

    m_ratioSummaryLabel = new QLabel();
    m_ratioSummaryLabel->setStyleSheet("color: gray;");
    ratioLayout->addWidget(m_ratioSummaryLabel);

    mainLayout->addWidget(ratioGroup);

    // === 目录结构选项 ===
    QGroupBox* structGroup = new QGroupBox("目录结构");
    QVBoxLayout* structLayout = new QVBoxLayout(structGroup);
    m_separateFoldersCheck = new QCheckBox("分开 images/labels 子文件夹");
    m_separateFoldersCheck->setChecked(true);  // 默认分开
    m_separateFoldersCheck->setToolTip(
        "勾选: train/images/ + train/labels/\n"
        "不勾选: train/ (图片和标签放一起)");
    structLayout->addWidget(m_separateFoldersCheck);

    m_excludeBackgroundCheck = new QCheckBox("仅导出有标注的图片（排除背景图片）");
    m_excludeBackgroundCheck->setChecked(false);
    m_excludeBackgroundCheck->setToolTip(
        "勾选: 只导出有非空 txt 标注文件的图片\n"
        "不勾选: 导出所有图片（无标注的作为背景/负样本）");
    structLayout->addWidget(m_excludeBackgroundCheck);

    mainLayout->addWidget(structGroup);

    // === 文件夹名称组 ===
    QGroupBox* folderGroup = new QGroupBox("文件夹名称");
    QFormLayout* folderLayout = new QFormLayout(folderGroup);

    m_trainFolderEdit = new QLineEdit("train");
    m_valFolderEdit = new QLineEdit("val");
    m_testFolderEdit = new QLineEdit("test");

    folderLayout->addRow("训练集:", m_trainFolderEdit);
    folderLayout->addRow("验证集:", m_valFolderEdit);
    folderLayout->addRow("测试集:", m_testFolderEdit);

    mainLayout->addWidget(folderGroup);

    // === YAML 配置组 ===
    QGroupBox* yamlGroup = new QGroupBox("YAML 配置");
    QFormLayout* yamlLayout = new QFormLayout(yamlGroup);

    m_datasetNameEdit = new QLineEdit("my_dataset");
    m_yamlNameEdit = new QLineEdit("dataset.yaml");

    yamlLayout->addRow("数据集名称:", m_datasetNameEdit);
    yamlLayout->addRow("YAML 文件名:", m_yamlNameEdit);

    mainLayout->addWidget(yamlGroup);

    // === 进度 ===
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color: gray;");
    mainLayout->addWidget(m_statusLabel);

    // === 按钮 ===
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_splitBtn = new QPushButton("开始分割");
    m_cancelBtn = new QPushButton("取消");
    m_splitBtn->setDefault(true);
    btnLayout->addStretch();
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_splitBtn);
    mainLayout->addLayout(btnLayout);

    // === 连接信号 ===
    connect(m_browseSourceBtn, &QPushButton::clicked, this, &SplitDatasetDialog::onBrowseSource);
    connect(m_browseOutputBtn, &QPushButton::clicked, this, &SplitDatasetDialog::onBrowseOutput);
    connect(m_testEnabledCheck, &QCheckBox::toggled, this, &SplitDatasetDialog::onTestEnabledChanged);
    connect(m_trainRatioSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SplitDatasetDialog::onRatioChanged);
    connect(m_valRatioSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SplitDatasetDialog::onRatioChanged);
    connect(m_testRatioSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SplitDatasetDialog::onRatioChanged);
    connect(m_splitBtn, &QPushButton::clicked, this, &SplitDatasetDialog::onSplit);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // 初始化
    updateRatioDisplay();
    onTestEnabledChanged(m_testEnabledCheck->isChecked());
}

void SplitDatasetDialog::onBrowseSource() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择图片文件夹",
        m_sourceImagesEdit->text());
    if (!dir.isEmpty()) {
        m_sourceImagesEdit->setText(dir);

        // 尝试自动检测 labels 文件夹
        QString labelsDir = dir;
        labelsDir.replace("/images", "/labels");
        labelsDir.replace("\\images", "\\labels");
        if (QDir(labelsDir).exists() && labelsDir != dir) {
            m_sourceLabelsEdit->setText(labelsDir);
        } else {
            m_sourceLabelsEdit->setText(dir);
        }

        // 自动设置输出路径
        QDir parentDir(dir);
        parentDir.cdUp();
        m_outputPathEdit->setText(parentDir.absolutePath() + "/split_dataset");
    }
}

void SplitDatasetDialog::onBrowseOutput() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择输出文件夹",
        m_outputPathEdit->text());
    if (!dir.isEmpty()) {
        m_outputPathEdit->setText(dir);
    }
}

void SplitDatasetDialog::onTestEnabledChanged(bool enabled) {
    m_testRatioSpin->setEnabled(enabled);
    m_testFolderEdit->setEnabled(enabled);
    if (!enabled) {
        m_testRatioSpin->setValue(0);
    } else if (m_testRatioSpin->value() == 0) {
        m_testRatioSpin->setValue(10);
    }
    updateRatioDisplay();
}

void SplitDatasetDialog::onRatioChanged() {
    updateRatioDisplay();
}

void SplitDatasetDialog::updateRatioDisplay() {
    int train = m_trainRatioSpin->value();
    int val = m_valRatioSpin->value();
    int test = m_testEnabledCheck->isChecked() ? m_testRatioSpin->value() : 0;
    int total = train + val + test;

    QString color = (total == 100) ? "green" : "red";
    m_ratioSummaryLabel->setStyleSheet(QString("color: %1;").arg(color));
    m_ratioSummaryLabel->setText(QString("总计: %1% %2")
        .arg(total)
        .arg(total == 100 ? "✓" : "(需要等于 100%)"));
}

bool SplitDatasetDialog::validateInputs() {
    if (m_sourceImagesEdit->text().isEmpty()) {
        QMessageBox::warning(this, "错误", "请选择图片文件夹");
        return false;
    }

    if (!QDir(m_sourceImagesEdit->text()).exists()) {
        QMessageBox::warning(this, "错误", "图片文件夹不存在");
        return false;
    }

    if (m_outputPathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "错误", "请选择输出文件夹");
        return false;
    }

    int total = m_trainRatioSpin->value() + m_valRatioSpin->value();
    if (m_testEnabledCheck->isChecked()) {
        total += m_testRatioSpin->value();
    }

    if (total != 100) {
        QMessageBox::warning(this, "错误", "分割比例总和必须等于 100%");
        return false;
    }

    return true;
}

void SplitDatasetDialog::onSplit() {
    if (!validateInputs()) return;

    m_splitBtn->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    performSplit();

    m_splitBtn->setEnabled(true);
}

void SplitDatasetDialog::performSplit() {
    QString imagesDir = m_sourceImagesEdit->text();
    QString labelsDir = m_sourceLabelsEdit->text();
    QString outputDir = m_outputPathEdit->text();

    // 如果 labels 目录为空，使用 images 同目录
    if (labelsDir.isEmpty()) {
        labelsDir = imagesDir;
    }

    // 获取所有图片文件
    QDir imgDir(imagesDir);
    QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.bmp"};
    QStringList imageFiles = imgDir.entryList(filters, QDir::Files);

    if (imageFiles.isEmpty()) {
        QMessageBox::warning(this, "错误", "没有找到图片文件");
        return;
    }

    // 如果需要排除背景图片，过滤掉没有非空标注的图片
    int backgroundCount = 0;
    if (m_excludeBackgroundCheck->isChecked()) {
        QStringList filteredFiles;
        for (const QString& imgName : imageFiles) {
            QString baseName = QFileInfo(imgName).completeBaseName();
            QString lblPath = labelsDir + "/" + baseName + ".txt";
            QFileInfo lblInfo(lblPath);

            // 检查标注文件是否存在且非空
            if (lblInfo.exists() && lblInfo.size() > 0) {
                filteredFiles.append(imgName);
            } else {
                backgroundCount++;
            }
        }
        imageFiles = filteredFiles;

        if (imageFiles.isEmpty()) {
            QMessageBox::warning(this, "错误", "没有找到有标注的图片文件");
            return;
        }
    }

    QString statusText = QString("找到 %1 张图片").arg(imageFiles.size());
    if (backgroundCount > 0) {
        statusText += QString("（已排除 %1 张背景图片）").arg(backgroundCount);
    }
    m_statusLabel->setText(statusText);
    QApplication::processEvents();

    // 随机打乱
    std::vector<QString> shuffled(imageFiles.begin(), imageFiles.end());
    auto rng = QRandomGenerator::global();
    std::shuffle(shuffled.begin(), shuffled.end(), *rng);

    // 计算分割数量
    int total = shuffled.size();
    int trainRatio = m_trainRatioSpin->value();
    int valRatio = m_valRatioSpin->value();
    int testRatio = m_testEnabledCheck->isChecked() ? m_testRatioSpin->value() : 0;

    int trainCount = total * trainRatio / 100;
    int valCount = total * valRatio / 100;
    int testCount = total - trainCount - valCount;

    // 是否分开 images/labels 子文件夹
    bool separateFolders = m_separateFoldersCheck->isChecked();

    // 创建输出目录结构
    QString trainImgDir, trainLblDir, valImgDir, valLblDir, testImgDir, testLblDir;
    if (separateFolders) {
        // 分开: train/images/ + train/labels/
        trainImgDir = outputDir + "/" + m_trainFolderEdit->text() + "/images";
        trainLblDir = outputDir + "/" + m_trainFolderEdit->text() + "/labels";
        valImgDir = outputDir + "/" + m_valFolderEdit->text() + "/images";
        valLblDir = outputDir + "/" + m_valFolderEdit->text() + "/labels";
        if (testRatio > 0) {
            testImgDir = outputDir + "/" + m_testFolderEdit->text() + "/images";
            testLblDir = outputDir + "/" + m_testFolderEdit->text() + "/labels";
        }
    } else {
        // 合并: train/ (图片和标签放一起)
        trainImgDir = outputDir + "/" + m_trainFolderEdit->text();
        trainLblDir = trainImgDir;  // 同一目录
        valImgDir = outputDir + "/" + m_valFolderEdit->text();
        valLblDir = valImgDir;
        if (testRatio > 0) {
            testImgDir = outputDir + "/" + m_testFolderEdit->text();
            testLblDir = testImgDir;
        }
    }

    QDir().mkpath(trainImgDir);
    if (separateFolders) QDir().mkpath(trainLblDir);
    QDir().mkpath(valImgDir);
    if (separateFolders) QDir().mkpath(valLblDir);
    if (testRatio > 0) {
        QDir().mkpath(testImgDir);
        if (separateFolders) QDir().mkpath(testLblDir);
    }

    m_progressBar->setMaximum(total);

    // 复制文件
    int idx = 0;
    auto copyFile = [&](const QString& imgName, const QString& destImgDir, const QString& destLblDir) {
        // 复制图片
        QString srcImg = imagesDir + "/" + imgName;
        QString dstImg = destImgDir + "/" + imgName;
        QFile::copy(srcImg, dstImg);

        // 复制标签
        QString baseName = QFileInfo(imgName).completeBaseName();
        QString srcLbl = labelsDir + "/" + baseName + ".txt";
        QString dstLbl = destLblDir + "/" + baseName + ".txt";
        if (QFile::exists(srcLbl)) {
            QFile::copy(srcLbl, dstLbl);
        }

        idx++;
        m_progressBar->setValue(idx);
        if (idx % 50 == 0) {
            m_statusLabel->setText(QString("复制中... %1/%2").arg(idx).arg(total));
            QApplication::processEvents();
        }
    };

    // 分配文件
    int i = 0;
    for (; i < trainCount; ++i) {
        copyFile(shuffled[i], trainImgDir, trainLblDir);
    }
    for (; i < trainCount + valCount; ++i) {
        copyFile(shuffled[i], valImgDir, valLblDir);
    }
    for (; i < total; ++i) {
        copyFile(shuffled[i], testImgDir, testLblDir);
    }

    // 查找源配置（优先 YAML，其次 classes.txt）
    DatasetConfig sourceConfig;
    QString yamlSrc;
    QString classesSrc;

    // 查找 YAML 文件
    QDir parent(imagesDir);
    parent.cdUp();
    QStringList yamlFilters = {"*.yaml", "*.yml"};

    // 检查上级目录
    QStringList yamls = parent.entryList(yamlFilters, QDir::Files);
    if (!yamls.isEmpty()) {
        yamlSrc = parent.absoluteFilePath(yamls.first());
    }

    // 检查图片目录
    if (yamlSrc.isEmpty()) {
        yamls = QDir(imagesDir).entryList(yamlFilters, QDir::Files);
        if (!yamls.isEmpty()) {
            yamlSrc = imagesDir + "/" + yamls.first();
        }
    }

    // 加载配置
    QStringList classNames;
    int kptCount = 0;
    bool hasPose = false;

    if (!yamlSrc.isEmpty() && sourceConfig.loadYAML(yamlSrc)) {
        classNames = sourceConfig.classNames();
        hasPose = sourceConfig.isPose();
        kptCount = sourceConfig.keypointCount();
    } else {
        // 回退到 classes.txt
        if (QFile::exists(imagesDir + "/classes.txt")) {
            classesSrc = imagesDir + "/classes.txt";
        } else if (QFile::exists(parent.absolutePath() + "/classes.txt")) {
            classesSrc = parent.absolutePath() + "/classes.txt";
        }

        if (!classesSrc.isEmpty()) {
            QFile::copy(classesSrc, outputDir + "/classes.txt");
            QFile classFile(classesSrc);
            if (classFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&classFile);
                while (!in.atEnd()) {
                    QString line = in.readLine().trimmed();
                    if (!line.isEmpty()) classNames.append(line);
                }
            }
        }
    }

    // 生成 YAML 文件
    QString yamlPath = outputDir + "/" + m_yamlNameEdit->text();
    QFile yamlFile(yamlPath);
    if (yamlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&yamlFile);
        out << "# " << m_datasetNameEdit->text() << " Dataset\n";
        out << "# Auto-generated by YoloAnnotator\n\n";
        out << "path: " << outputDir << "\n";

        // 根据目录结构设置路径
        if (separateFolders) {
            out << "train: " << m_trainFolderEdit->text() << "/images\n";
            out << "val: " << m_valFolderEdit->text() << "/images\n";
            if (testRatio > 0) {
                out << "test: " << m_testFolderEdit->text() << "/images\n";
            }
        } else {
            out << "train: " << m_trainFolderEdit->text() << "\n";
            out << "val: " << m_valFolderEdit->text() << "\n";
            if (testRatio > 0) {
                out << "test: " << m_testFolderEdit->text() << "\n";
            }
        }
        out << "\n";
        out << "# Classes\n";
        out << "nc: " << classNames.size() << "\n";
        out << "names:\n";
        for (int j = 0; j < classNames.size(); ++j) {
            out << "  " << j << ": " << classNames[j] << "\n";
        }

        // 如果有关键点配置，也输出
        if (hasPose && kptCount > 0) {
            out << "\n# Keypoint configuration\n";
            out << "kpt_shape: [" << kptCount << ", 3]\n";

            // 输出 flip_idx
            out << "flip_idx: [";
            for (int j = 0; j < kptCount; ++j) {
                if (j > 0) out << ", ";
                out << j;
            }
            out << "]\n";

            // 如果源配置有骨架，复制骨架配置
            const auto& bones = sourceConfig.skeletonConfig().bones();
            if (!bones.isEmpty()) {
                out << "\nskeleton:\n";
                for (const auto& bone : bones) {
                    out << "  - [" << (bone.from + 1) << ", " << (bone.to + 1) << "]\n";
                }
            }
        }
    }

    m_progressBar->setValue(total);
    m_statusLabel->setText(QString("完成! 训练:%1 验证:%2 测试:%3")
        .arg(trainCount).arg(valCount).arg(testCount));

    QMessageBox::information(this, "分割完成",
        QString("数据集分割完成!\n\n"
                "训练集: %1 张\n"
                "验证集: %2 张\n"
                "测试集: %3 张\n\n"
                "YAML: %4")
        .arg(trainCount).arg(valCount).arg(testCount).arg(yamlPath));

    accept();
}
