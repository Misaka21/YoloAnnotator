#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>

/**
 * @brief 数据集分割对话框
 *
 * 将数据集分割为训练集、验证集、测试集（可选）
 */
class SplitDatasetDialog : public QDialog {
    Q_OBJECT
public:
    explicit SplitDatasetDialog(QWidget* parent = nullptr);

    // 设置源路径（从当前打开的文件夹）
    void setSourcePath(const QString& imagesPath, const QString& labelsPath);

private slots:
    void onBrowseSource();
    void onBrowseOutput();
    void onTestEnabledChanged(bool enabled);
    void onRatioChanged();
    void onSplit();

private:
    void setupUI();
    void updateRatioDisplay();
    bool validateInputs();
    void performSplit();

    // 源路径
    QLineEdit* m_sourceImagesEdit;
    QLineEdit* m_sourceLabelsEdit;
    QPushButton* m_browseSourceBtn;

    // 输出路径
    QLineEdit* m_outputPathEdit;
    QPushButton* m_browseOutputBtn;

    // 分割比例
    QSpinBox* m_trainRatioSpin;
    QSpinBox* m_valRatioSpin;
    QSpinBox* m_testRatioSpin;
    QCheckBox* m_testEnabledCheck;
    QLabel* m_ratioSummaryLabel;

    // 目录结构
    QCheckBox* m_separateFoldersCheck;  // 是否分开 images/labels 子文件夹

    // 过滤选项
    QCheckBox* m_excludeBackgroundCheck;  // 排除无标注的背景图片

    // 文件夹名称
    QLineEdit* m_trainFolderEdit;
    QLineEdit* m_valFolderEdit;
    QLineEdit* m_testFolderEdit;

    // YAML 配置
    QLineEdit* m_yamlNameEdit;
    QLineEdit* m_datasetNameEdit;

    // 进度
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;

    // 按钮
    QPushButton* m_splitBtn;
    QPushButton* m_cancelBtn;
};
