#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>

class YoloDetector;

/**
 * @brief 模型设置对话框
 */
class ModelSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelSettingsDialog(YoloDetector* detector, QWidget* parent = nullptr);

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

    void setupUI();
    void updateModelInfo();
};
