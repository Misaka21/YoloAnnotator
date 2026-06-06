#include "ImageEnhancementPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

ImageEnhancementPanel::ImageEnhancementPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void ImageEnhancementPanel::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 亮度
    QHBoxLayout* brightnessLayout = new QHBoxLayout();
    QLabel* brightnessTitle = new QLabel("亮度:");
    m_brightnessSlider = new QSlider(Qt::Horizontal);
    m_brightnessSlider->setRange(-100, 100);
    m_brightnessSlider->setValue(0);
    m_brightnessSlider->setTickPosition(QSlider::NoTicks);
    m_brightnessLabel = new QLabel("0");
    m_brightnessLabel->setFixedWidth(35);
    m_brightnessLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    brightnessLayout->addWidget(brightnessTitle);
    brightnessLayout->addWidget(m_brightnessSlider);
    brightnessLayout->addWidget(m_brightnessLabel);

    // 对比度
    QHBoxLayout* contrastLayout = new QHBoxLayout();
    QLabel* contrastTitle = new QLabel("对比度:");
    m_contrastSlider = new QSlider(Qt::Horizontal);
    m_contrastSlider->setRange(0, 200);  // 0.0 ~ 2.0 (显示为百分比)
    m_contrastSlider->setValue(100);      // 默认 1.0
    m_contrastSlider->setTickPosition(QSlider::NoTicks);
    m_contrastLabel = new QLabel("1.0");
    m_contrastLabel->setFixedWidth(35);
    m_contrastLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    contrastLayout->addWidget(contrastTitle);
    contrastLayout->addWidget(m_contrastSlider);
    contrastLayout->addWidget(m_contrastLabel);

    // 饱和度
    QHBoxLayout* saturationLayout = new QHBoxLayout();
    QLabel* saturationTitle = new QLabel("饱和度:");
    m_saturationSlider = new QSlider(Qt::Horizontal);
    m_saturationSlider->setRange(0, 200);   // 0.0 ~ 2.0 (显示为百分比)
    m_saturationSlider->setValue(100);       // 默认 1.0
    m_saturationSlider->setTickPosition(QSlider::NoTicks);
    m_saturationLabel = new QLabel("1.0");
    m_saturationLabel->setFixedWidth(35);
    m_saturationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    saturationLayout->addWidget(saturationTitle);
    saturationLayout->addWidget(m_saturationSlider);
    saturationLayout->addWidget(m_saturationLabel);

    // 重置按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_resetBtn = new QPushButton("重置");
    m_resetBtn->setFixedWidth(50);
    btnLayout->addWidget(m_resetBtn);
    btnLayout->addStretch();

    mainLayout->addLayout(brightnessLayout);
    mainLayout->addLayout(contrastLayout);
    mainLayout->addLayout(saturationLayout);
    mainLayout->addLayout(btnLayout);

    // 信号连接
    connect(m_brightnessSlider, &QSlider::valueChanged, [this](int v) {
        m_brightnessLabel->setText(QString::number(v));
        emit brightnessChanged(v);
    });
    connect(m_contrastSlider, &QSlider::valueChanged, [this](int v) {
        double val = v / 100.0;
        m_contrastLabel->setText(QString::number(val, 'f', 1));
        emit contrastChanged(v);
    });
    connect(m_saturationSlider, &QSlider::valueChanged, [this](int v) {
        double val = v / 100.0;
        m_saturationLabel->setText(QString::number(val, 'f', 1));
        emit saturationChanged(v);
    });
    connect(m_resetBtn, &QPushButton::clicked, this, &ImageEnhancementPanel::reset);
}

void ImageEnhancementPanel::reset() {
    m_brightnessSlider->blockSignals(true);
    m_contrastSlider->blockSignals(true);
    m_saturationSlider->blockSignals(true);

    m_brightnessSlider->setValue(0);
    m_contrastSlider->setValue(100);
    m_saturationSlider->setValue(100);

    m_brightnessSlider->blockSignals(false);
    m_contrastSlider->blockSignals(false);
    m_saturationSlider->blockSignals(false);

    m_brightnessLabel->setText("0");
    m_contrastLabel->setText("1.0");
    m_saturationLabel->setText("1.0");

    emit brightnessChanged(0);
    emit contrastChanged(100);
    emit saturationChanged(100);
}
