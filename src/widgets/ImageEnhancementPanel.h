#pragma once
#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>

/**
 * @brief 图像增强面板 - 调整亮度/对比度/饱和度（仅显示，不修改原图）
 */
class ImageEnhancementPanel : public QWidget {
    Q_OBJECT
public:
    explicit ImageEnhancementPanel(QWidget* parent = nullptr);

    int brightness() const { return m_brightnessSlider->value(); }
    int contrast() const { return m_contrastSlider->value(); }
    int saturation() const { return m_saturationSlider->value(); }

    void reset();

signals:
    void brightnessChanged(int value);
    void contrastChanged(int value);
    void saturationChanged(int value);

private:
    QSlider* m_brightnessSlider;
    QSlider* m_contrastSlider;
    QSlider* m_saturationSlider;
    QLabel* m_brightnessLabel;
    QLabel* m_contrastLabel;
    QLabel* m_saturationLabel;
    QPushButton* m_resetBtn;

    void setupUI();
};
