#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include <QButtonGroup>
#include "io/DatasetConfig.h"

/**
 * @brief 新建项目对话框
 *
 * 用于从 classes.txt 创建 YAML 项目文件
 */
class NewProjectDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewProjectDialog(QWidget* parent = nullptr);

    // 设置初始类别列表（从 classes.txt 加载）
    void setClassNames(const QStringList& names);

    // 获取配置结果
    DatasetConfig config() const { return m_config; }
    QString projectSavePath() const { return m_savePathEdit->text(); }
    QString imageFolderPath() const { return m_imagePathEdit->text(); }

private slots:
    void onBrowseImages();
    void onBrowseSavePath();
    void onTaskTypeChanged();
    void onAccept();

private:
    void setupUI();
    void validateInputs();

    // 输入控件
    QLineEdit* m_imagePathEdit;
    QPushButton* m_browseImagesBtn;

    QRadioButton* m_detectRadio;
    QRadioButton* m_poseRadio;
    QButtonGroup* m_taskGroup;

    QSpinBox* m_keypointSpinBox;
    QLabel* m_keypointLabel;
    QComboBox* m_kptDimCombo;
    QLabel* m_kptDimLabel;

    QLineEdit* m_savePathEdit;
    QPushButton* m_browseSaveBtn;

    QLabel* m_classesInfoLabel;

    QPushButton* m_createBtn;
    QPushButton* m_cancelBtn;

    // 数据
    QStringList m_classNames;
    DatasetConfig m_config;
};
