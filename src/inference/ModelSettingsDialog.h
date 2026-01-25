#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QScrollArea>
#include <QVBoxLayout>

class YoloDetector;

/**
 * @brief 模型设置对话框
 */
class ModelSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelSettingsDialog(YoloDetector* detector, QWidget* parent = nullptr);

    // 设置数据集类别（用于映射下拉框）
    void setDatasetClasses(const QStringList& classes);

private slots:
    void onBrowseModel();
    void onLoadModel();
    void onApplySettings();

private:
    YoloDetector* m_detector;

    // 模型选择
    QLineEdit* m_modelPathEdit;
    QPushButton* m_browseBtn;
    QPushButton* m_loadBtn;

    // 模型信息
    QLabel* m_modelTypeLabel;
    QLabel* m_inputSizeLabel;
    QLabel* m_classesLabel;

    // 推理参数
    QDoubleSpinBox* m_confSpinBox;
    QDoubleSpinBox* m_iouSpinBox;
    QComboBox* m_backendCombo;

    // 类别映射
    QGroupBox* m_mappingGroup;
    QWidget* m_mappingContainer;
    QVBoxLayout* m_mappingLayout;
    QVector<QCheckBox*> m_enableChecks;
    QVector<QComboBox*> m_targetCombos;
    QStringList m_datasetClasses;

    void setupUI();
    void updateModelInfo();
    void rebuildMappingUI();  // 重建类别映射 UI
};
