#include "BatchRenameDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QHeaderView>
#include <QProgressDialog>
#include <QApplication>
#include <QCollator>
#include <QRegularExpression>

BatchRenameDialog::BatchRenameDialog(QWidget* parent)
    : QDialog(parent) {
    setupUI();
    setWindowTitle("批量重命名");
    resize(700, 600);
}

void BatchRenameDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ========== 项目信息 ==========
    QGroupBox* infoGroup = new QGroupBox("项目信息", this);
    QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);
    m_projectInfoLabel = new QLabel("未加载项目", this);
    m_projectInfoLabel->setWordWrap(true);
    infoLayout->addWidget(m_projectInfoLabel);
    mainLayout->addWidget(infoGroup);

    // ========== 命名规则 ==========
    QGroupBox* patternGroup = new QGroupBox("命名规则", this);
    QFormLayout* patternLayout = new QFormLayout(patternGroup);

    m_prefixEdit = new QLineEdit("image", this);
    m_prefixEdit->setPlaceholderText("例如：IMG, photo, frame 等");
    patternLayout->addRow("前缀:", m_prefixEdit);

    m_startNumberSpinBox = new QSpinBox(this);
    m_startNumberSpinBox->setRange(0, 999999);
    m_startNumberSpinBox->setValue(1);
    patternLayout->addRow("起始序号:", m_startNumberSpinBox);

    m_digitsSpinBox = new QSpinBox(this);
    m_digitsSpinBox->setRange(3, 8);
    m_digitsSpinBox->setValue(6);
    patternLayout->addRow("序号位数:", m_digitsSpinBox);

    QLabel* exampleLabel = new QLabel(this);
    exampleLabel->setStyleSheet("QLabel { color: #666; font-style: italic; }");
    patternLayout->addRow("示例:", exampleLabel);

    mainLayout->addWidget(patternGroup);

    // ========== 预览列表 ==========
    QGroupBox* previewGroup = new QGroupBox("重命名预览", this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);

    m_previewTable = new QTableWidget(this);
    m_previewTable->setColumnCount(3);
    m_previewTable->setHorizontalHeaderLabels({"序号", "原文件名", "新文件名"});
    m_previewTable->horizontalHeader()->setStretchLastSection(true);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_previewTable->setAlternatingRowColors(true);
    m_previewTable->setColumnWidth(0, 60);
    m_previewTable->setColumnWidth(1, 250);
    previewLayout->addWidget(m_previewTable);

    mainLayout->addWidget(previewGroup);

    // ========== 状态信息 ==========
    m_statusLabel = new QLabel("请设置命名规则", this);
    mainLayout->addWidget(m_statusLabel);

    // ========== 按钮 ==========
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_renameBtn = new QPushButton("执行重命名", this);
    m_renameBtn->setEnabled(false);
    m_closeBtn = new QPushButton("关闭", this);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_renameBtn);
    buttonLayout->addWidget(m_closeBtn);
    mainLayout->addLayout(buttonLayout);

    // ========== 信号连接 ==========
    connect(m_prefixEdit, &QLineEdit::textChanged, this, &BatchRenameDialog::onPatternChanged);
    connect(m_startNumberSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BatchRenameDialog::onPatternChanged);
    connect(m_digitsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BatchRenameDialog::onPatternChanged);
    connect(m_renameBtn, &QPushButton::clicked, this, &BatchRenameDialog::onRename);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    // 更新示例标签
    connect(m_prefixEdit, &QLineEdit::textChanged, [this, exampleLabel]() {
        exampleLabel->setText(generateNewName(0, ".jpg"));
    });
    connect(m_startNumberSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            [this, exampleLabel]() {
        exampleLabel->setText(generateNewName(0, ".jpg"));
    });
    connect(m_digitsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            [this, exampleLabel]() {
        exampleLabel->setText(generateNewName(0, ".jpg"));
    });
}

void BatchRenameDialog::setProjectPath(const QString& imagePath, const QString& labelsPath) {
    m_imagePath = imagePath;
    m_labelsPath = labelsPath;

    QString info = QString("图片路径: %1\n标注路径: %2")
        .arg(imagePath)
        .arg(labelsPath.isEmpty() ? "无" : labelsPath);
    m_projectInfoLabel->setText(info);
}

void BatchRenameDialog::setImageFiles(const QStringList& imageFiles) {
    // 按自然数字序排序（1,2,3...而非1,10,2...）
    m_imageFiles = imageFiles;
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(m_imageFiles.begin(), m_imageFiles.end(),
        [&collator](const QString& a, const QString& b) {
            return collator.compare(a, b) < 0;
        });

    m_statusLabel->setText(QString("共 %1 个文件").arg(m_imageFiles.size()));

    // 自动预览
    onPatternChanged();
}

QString BatchRenameDialog::generateNewName(int index, const QString& extension) {
    int number = m_startNumberSpinBox->value() + index;
    QString prefix = m_prefixEdit->text();
    int digits = m_digitsSpinBox->value();

    return QString("%1_%2%3")
        .arg(prefix)
        .arg(number, digits, 10, QChar('0'))
        .arg(extension);
}

void BatchRenameDialog::onPatternChanged() {
    updatePreview();
}

void BatchRenameDialog::onPreview() {
    updatePreview();
}

void BatchRenameDialog::updatePreview() {
    if (m_imageFiles.isEmpty()) {
        m_renameBtn->setEnabled(false);
        return;
    }

    // 检查前缀是否有效
    QString prefix = m_prefixEdit->text();
    if (prefix.isEmpty() || prefix.contains(QRegularExpression("[\\\\/:*?\"<>|]"))) {
        m_statusLabel->setText("错误：前缀不能为空或包含非法字符");
        m_renameBtn->setEnabled(false);
        return;
    }

    m_previewTable->setRowCount(m_imageFiles.size());

    for (int i = 0; i < m_imageFiles.size(); ++i) {
        QString oldName = m_imageFiles[i];
        QFileInfo fileInfo(oldName);
        QString extension = "." + fileInfo.suffix();
        QString newName = generateNewName(i, extension);

        m_previewTable->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        m_previewTable->setItem(i, 1, new QTableWidgetItem(oldName));
        m_previewTable->setItem(i, 2, new QTableWidgetItem(newName));
    }

    m_statusLabel->setText(QString("准备重命名 %1 个图片文件和对应标注文件").arg(m_imageFiles.size()));
    m_renameBtn->setEnabled(true);
}

void BatchRenameDialog::onRename() {
    if (m_imageFiles.isEmpty()) {
        return;
    }

    // 确认对话框
    auto reply = QMessageBox::question(
        this,
        "确认重命名",
        QString("即将重命名 %1 个图片文件和对应的标注文件。\n\n"
                "此操作不可恢复，确定继续？")
            .arg(m_imageFiles.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    // 创建进度对话框
    QProgressDialog progress("正在重命名文件...", "取消", 0, m_imageFiles.size() * 2, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    int progressValue = 0;

    // 第一步：重命名为临时名称（避免冲突）
    QStringList tempImageNames;
    QStringList tempLabelNames;
    QStringList finalImageNames;
    QStringList finalLabelNames;

    for (int i = 0; i < m_imageFiles.size(); ++i) {
        if (progress.wasCanceled()) {
            QMessageBox::warning(this, "已取消", "重命名操作已取消");
            return;
        }

        QString oldImageName = m_imageFiles[i];
        QFileInfo imageInfo(oldImageName);
        QString extension = "." + imageInfo.suffix();

        QString newImageName = generateNewName(i, extension);
        QString tempImageName = QString("__temp_%1%2").arg(i, 8, 10, QChar('0')).arg(extension);

        QString oldImagePath = m_imagePath + "/" + oldImageName;
        QString tempImagePath = m_imagePath + "/" + tempImageName;

        // 重命名图片为临时名称
        if (!QFile::rename(oldImagePath, tempImagePath)) {
            QMessageBox::critical(this, "错误",
                QString("重命名图片失败：%1").arg(oldImageName));
            return;
        }

        tempImageNames.append(tempImageName);
        finalImageNames.append(newImageName);

        // 处理标注文件
        if (!m_labelsPath.isEmpty()) {
            QString baseName = imageInfo.completeBaseName();
            QString oldLabelPath = m_labelsPath + "/" + baseName + ".txt";

            if (QFile::exists(oldLabelPath)) {
                QString tempLabelName = QString("__temp_%1.txt").arg(i, 8, 10, QChar('0'));
                QString tempLabelPath = m_labelsPath + "/" + tempLabelName;

                if (!QFile::rename(oldLabelPath, tempLabelPath)) {
                    QMessageBox::critical(this, "错误",
                        QString("重命名标注文件失败：%1").arg(baseName + ".txt"));
                    return;
                }

                tempLabelNames.append(tempLabelName);

                QString newBaseName = QFileInfo(newImageName).completeBaseName();
                finalLabelNames.append(newBaseName + ".txt");
            } else {
                tempLabelNames.append("");
                finalLabelNames.append("");
            }
        }

        progress.setValue(++progressValue);
        QApplication::processEvents();
    }

    // 第二步：重命名为最终名称
    for (int i = 0; i < tempImageNames.size(); ++i) {
        if (progress.wasCanceled()) {
            QMessageBox::warning(this, "已取消", "重命名操作已取消");
            return;
        }

        QString tempImagePath = m_imagePath + "/" + tempImageNames[i];
        QString finalImagePath = m_imagePath + "/" + finalImageNames[i];

        if (!QFile::rename(tempImagePath, finalImagePath)) {
            QMessageBox::critical(this, "错误",
                QString("重命名图片失败：%1 -> %2")
                    .arg(tempImageNames[i]).arg(finalImageNames[i]));
            return;
        }

        // 处理标注文件
        if (!m_labelsPath.isEmpty() && !tempLabelNames[i].isEmpty()) {
            QString tempLabelPath = m_labelsPath + "/" + tempLabelNames[i];
            QString finalLabelPath = m_labelsPath + "/" + finalLabelNames[i];

            if (!QFile::rename(tempLabelPath, finalLabelPath)) {
                QMessageBox::critical(this, "错误",
                    QString("重命名标注文件失败：%1 -> %2")
                        .arg(tempLabelNames[i]).arg(finalLabelNames[i]));
                return;
            }
        }

        progress.setValue(++progressValue);
        QApplication::processEvents();
    }

    progress.setValue(m_imageFiles.size() * 2);

    QMessageBox::information(this, "完成",
        QString("成功重命名 %1 个图片文件和对应的标注文件！\n\n"
                "请重新打开项目以刷新文件列表。")
            .arg(m_imageFiles.size()));

    accept();
}
