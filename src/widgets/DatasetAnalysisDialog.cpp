#include "DatasetAnalysisDialog.h"
#include "io/AnnotationIO.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QTabWidget>
#include <QDir>
#include <QPainter>
#include <QApplication>
#include <cmath>

// ==================== BarChartWidget ====================

BarChartWidget::BarChartWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(300, 200);
}

void BarChartWidget::setData(const QMap<int, int>& classCount, const QStringList& classNames) {
    m_classCount = classCount;
    m_classNames = classNames;
    update();
}

void BarChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_classCount.isEmpty()) {
        p.drawText(rect(), Qt::AlignCenter, "No data");
        return;
    }

    // 计算最大值
    int maxCount = 0;
    for (int count : m_classCount) {
        maxCount = qMax(maxCount, count);
    }
    if (maxCount == 0) maxCount = 1;

    // 边距
    const int marginLeft = 50;
    const int marginRight = 20;
    const int marginTop = 30;
    const int marginBottom = 50;

    int chartWidth = width() - marginLeft - marginRight;
    int chartHeight = height() - marginTop - marginBottom;

    // 绘制坐标轴
    p.setPen(Qt::black);
    p.drawLine(marginLeft, height() - marginBottom, width() - marginRight, height() - marginBottom);
    p.drawLine(marginLeft, marginTop, marginLeft, height() - marginBottom);

    // Y轴标签
    p.setFont(QFont("Segoe UI", 8));
    for (int i = 0; i <= 5; ++i) {
        int y = height() - marginBottom - (chartHeight * i / 5);
        int val = maxCount * i / 5;
        p.drawText(0, y - 8, marginLeft - 5, 16, Qt::AlignRight | Qt::AlignVCenter, QString::number(val));
        p.setPen(QColor(200, 200, 200));
        p.drawLine(marginLeft, y, width() - marginRight, y);
        p.setPen(Qt::black);
    }

    // 绘制柱状图
    int barCount = m_classCount.size();
    if (barCount == 0) return;

    double barWidth = double(chartWidth) / barCount * 0.7;
    double spacing = double(chartWidth) / barCount;

    // 颜色列表（类似用户提供的图片）
    QList<QColor> colors = {
        QColor(0, 0, 255),      // 蓝色
        QColor(0, 255, 255),    // 青色
        QColor(200, 200, 200),  // 灰色
        QColor(0, 255, 128),    // 青绿
        QColor(255, 128, 200),  // 粉色
        QColor(0, 0, 128),      // 深蓝
        QColor(255, 0, 0),      // 红色
        QColor(255, 255, 0),    // 黄色
        QColor(0, 255, 0),      // 绿色
        QColor(255, 0, 255),    // 洋红
        QColor(0, 200, 255),    // 天蓝
        QColor(255, 128, 0),    // 橙色
    };

    int i = 0;
    for (auto it = m_classCount.begin(); it != m_classCount.end(); ++it, ++i) {
        int classId = it.key();
        int count = it.value();

        double x = marginLeft + spacing * i + (spacing - barWidth) / 2;
        double barHeight = double(count) / maxCount * chartHeight;
        double y = height() - marginBottom - barHeight;

        QColor color = colors[i % colors.size()];
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawRect(QRectF(x, y, barWidth, barHeight));

        // 数值标签
        p.setPen(Qt::black);
        p.setFont(QFont("Segoe UI", 8));
        p.drawText(QRectF(x, y - 18, barWidth, 16), Qt::AlignCenter, QString::number(count));

        // X轴类别名
        QString label = (classId < m_classNames.size()) ? m_classNames[classId] : QString::number(classId);
        // 截断长名称
        if (label.length() > 6) label = label.left(5) + "..";
        p.drawText(QRectF(x - 5, height() - marginBottom + 5, barWidth + 10, 40),
                   Qt::AlignHCenter | Qt::AlignTop, label);
    }

    // Y轴标题
    p.save();
    p.translate(15, height() / 2);
    p.rotate(-90);
    p.drawText(QRect(-50, -10, 100, 20), Qt::AlignCenter, "instances");
    p.restore();
}

// ==================== HeatmapWidget ====================

HeatmapWidget::HeatmapWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
}

void HeatmapWidget::setPoints(const QVector<QPointF>& points) {
    m_points = points;
    update();
}

void HeatmapWidget::setAxisLabels(const QString& xLabel, const QString& yLabel) {
    m_xLabel = xLabel;
    m_yLabel = yLabel;
    update();
}

void HeatmapWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int marginLeft = 40;
    const int marginRight = 10;
    const int marginTop = 10;
    const int marginBottom = 40;

    // 使用正方形区域
    int availableWidth = width() - marginLeft - marginRight;
    int availableHeight = height() - marginTop - marginBottom;
    int chartSize = qMin(availableWidth, availableHeight);

    // 居中显示
    int offsetX = marginLeft + (availableWidth - chartSize) / 2;
    int offsetY = marginTop + (availableHeight - chartSize) / 2;

    // 绘制背景
    p.fillRect(offsetX, offsetY, chartSize, chartSize, Qt::white);

    if (m_points.isEmpty()) {
        p.drawText(rect(), Qt::AlignCenter, "No data");
        return;
    }

    // 构建热力图网格
    QVector<QVector<int>> grid(GRID_SIZE, QVector<int>(GRID_SIZE, 0));
    int maxDensity = 0;

    for (const QPointF& pt : m_points) {
        int gx = qBound(0, int(pt.x() * GRID_SIZE), GRID_SIZE - 1);
        int gy = qBound(0, int(pt.y() * GRID_SIZE), GRID_SIZE - 1);
        grid[gy][gx]++;
        maxDensity = qMax(maxDensity, grid[gy][gx]);
    }

    if (maxDensity == 0) maxDensity = 1;

    // 创建热力图图像
    QImage heatmap(GRID_SIZE, GRID_SIZE, QImage::Format_ARGB32);
    heatmap.fill(Qt::white);

    double sqrtMax = std::sqrt(maxDensity);

    for (int gy = 0; gy < GRID_SIZE; ++gy) {
        for (int gx = 0; gx < GRID_SIZE; ++gx) {
            if (grid[gy][gx] > 0) {
                // 使用平方根缩放，增强低密度区域可见度
                double intensity = std::sqrt(grid[gy][gx]) / sqrtMax;

                // 白色到深蓝渐变 (加深版)
                // 白色 (255,255,255) -> 浅蓝 (180,200,255) -> 深蓝 (0,30,80)
                int r, g, b;
                if (intensity < 0.5) {
                    double t = intensity * 2;  // 0-1
                    r = int(255 - 75 * t);     // 255 -> 180
                    g = int(255 - 55 * t);     // 255 -> 200
                    b = 255;                    // 保持255
                } else {
                    double t = (intensity - 0.5) * 2;  // 0-1
                    r = int(180 - 180 * t);    // 180 -> 0
                    g = int(200 - 170 * t);    // 200 -> 30
                    b = int(255 - 175 * t);    // 255 -> 80
                }

                // Y轴反转：gy=0 对应图像底部
                heatmap.setPixelColor(gx, GRID_SIZE - 1 - gy, QColor(r, g, b));
            }
        }
    }

    // 缩放并绘制热力图（使用最近邻插值保持方块清晰）
    QRect chartRect(offsetX, offsetY, chartSize, chartSize);
    p.drawImage(chartRect, heatmap.scaled(chartSize, chartSize, Qt::IgnoreAspectRatio, Qt::FastTransformation));

    // 绘制边框
    p.setPen(QColor(200, 200, 200));
    p.setBrush(Qt::NoBrush);
    p.drawRect(offsetX, offsetY, chartSize, chartSize);

    // 坐标轴标签
    p.setPen(Qt::black);
    p.setFont(QFont("Segoe UI", 8));

    // X轴
    for (int i = 0; i <= 4; ++i) {
        double val = i * 0.25;
        int x = offsetX + chartSize * i / 4;
        p.drawText(QRect(x - 20, offsetY + chartSize + 5, 40, 15),
                   Qt::AlignCenter, QString::number(val, 'f', 1));
    }
    p.drawText(QRect(offsetX, offsetY + chartSize + 20, chartSize, 20),
               Qt::AlignCenter, m_xLabel);

    // Y轴
    for (int i = 0; i <= 4; ++i) {
        double val = i * 0.25;
        int y = offsetY + chartSize - chartSize * i / 4;
        p.drawText(QRect(0, y - 8, offsetX - 5, 16),
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(val, 'f', 1));
    }

    p.save();
    p.translate(12, offsetY + chartSize / 2);
    p.rotate(-90);
    p.drawText(QRect(-40, -10, 80, 20), Qt::AlignCenter, m_yLabel);
    p.restore();
}

// ==================== BoxOverlayWidget ====================

BoxOverlayWidget::BoxOverlayWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
}

void BoxOverlayWidget::setBoxes(const QVector<QRectF>& boxes, const QVector<int>& classIds,
                                 const QStringList& classNames) {
    m_boxes = boxes;
    m_classIds = classIds;
    m_classNames = classNames;
    update();
}

QColor BoxOverlayWidget::getColorForClass(int classId, const QString& className) const {
    QString lowerName = className.toLower();
    if (lowerName.startsWith("r-") || lowerName.startsWith("r_") || lowerName.startsWith("r")) {
        return QColor(255, 0, 255, 100);  // 洋红
    } else if (lowerName.startsWith("b-") || lowerName.startsWith("b_") || lowerName.startsWith("b")) {
        return QColor(0, 255, 255, 100);  // 青色
    }
    // 默认按 classId 分配颜色
    QList<QColor> colors = {
        QColor(0, 0, 255, 100),
        QColor(0, 255, 255, 100),
        QColor(255, 0, 255, 100),
        QColor(0, 255, 0, 100),
        QColor(255, 255, 0, 100),
        QColor(255, 128, 0, 100),
    };
    return colors[classId % colors.size()];
}

void BoxOverlayWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int margin = 20;
    int chartSize = qMin(width(), height()) - margin * 2;
    int offsetX = (width() - chartSize) / 2;
    int offsetY = (height() - chartSize) / 2;

    // 绘制背景
    p.fillRect(offsetX, offsetY, chartSize, chartSize, Qt::white);
    p.setPen(Qt::black);
    p.drawRect(offsetX, offsetY, chartSize, chartSize);

    if (m_boxes.isEmpty()) {
        p.drawText(rect(), Qt::AlignCenter, "No data");
        return;
    }

    // 图表中心点
    double chartCenterX = offsetX + chartSize / 2.0;
    double chartCenterY = offsetY + chartSize / 2.0;

    // 绘制所有边界框（居中叠加，只绘制轮廓）
    for (int i = 0; i < m_boxes.size(); ++i) {
        const QRectF& box = m_boxes[i];
        int classId = (i < m_classIds.size()) ? m_classIds[i] : 0;
        QString className = (classId < m_classNames.size()) ? m_classNames[classId] : "";

        QColor color = getColorForClass(classId, className);
        color.setAlpha(150);  // 半透明轮廓

        // 边界框尺寸（归一化 -> 像素）
        double w = box.width() * chartSize;
        double h = box.height() * chartSize;

        // 居中绘制：所有框的中心对齐到图表中心
        double x = chartCenterX - w / 2.0;
        double y = chartCenterY - h / 2.0;

        p.setPen(QPen(color, 1));
        p.setBrush(Qt::NoBrush);  // 只绘制轮廓
        p.drawRect(QRectF(x, y, w, h));
    }
}

// ==================== ScaleDistributionWidget ====================

ScaleDistributionWidget::ScaleDistributionWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(300, 250);
}

void ScaleDistributionWidget::setData(const QMap<int, QVector<int>>& scaleData,
                                       const QStringList& classNames,
                                       const QSet<int>& visibleClasses) {
    m_scaleData = scaleData;
    m_classNames = classNames;
    m_visibleClasses = visibleClasses;
    update();
}

void ScaleDistributionWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 收集可见类别
    QVector<int> visibleIds;
    for (auto it = m_scaleData.begin(); it != m_scaleData.end(); ++it) {
        if (m_visibleClasses.contains(it.key()))
            visibleIds.append(it.key());
    }

    if (visibleIds.isEmpty()) {
        p.drawText(rect(), Qt::AlignCenter, "No data");
        return;
    }

    const int marginLeft = 50;
    const int marginRight = 20;
    const int marginTop = 30;
    const int marginBottom = 50;
    const int legendHeight = 25;

    int chartWidth = width() - marginLeft - marginRight;
    int chartHeight = height() - marginTop - marginBottom - legendHeight;

    // 计算最大总数
    int maxTotal = 0;
    for (int classId : visibleIds) {
        const QVector<int>& counts = m_scaleData[classId];
        int total = counts[0] + counts[1] + counts[2];
        maxTotal = qMax(maxTotal, total);
    }
    if (maxTotal == 0) maxTotal = 1;

    // 颜色: Small=蓝, Medium=绿, Large=橙
    const QColor colorSmall(66, 133, 244);
    const QColor colorMedium(52, 168, 83);
    const QColor colorLarge(251, 153, 2);

    // 绘制坐标轴
    p.setPen(Qt::black);
    p.drawLine(marginLeft, height() - marginBottom - legendHeight,
               width() - marginRight, height() - marginBottom - legendHeight);
    p.drawLine(marginLeft, marginTop, marginLeft, height() - marginBottom - legendHeight);

    // Y轴标签
    p.setFont(QFont("Segoe UI", 8));
    for (int i = 0; i <= 5; ++i) {
        int y = height() - marginBottom - legendHeight - (chartHeight * i / 5);
        int val = maxTotal * i / 5;
        p.drawText(0, y - 8, marginLeft - 5, 16, Qt::AlignRight | Qt::AlignVCenter, QString::number(val));
        p.setPen(QColor(200, 200, 200));
        p.drawLine(marginLeft, y, width() - marginRight, y);
        p.setPen(Qt::black);
    }

    // 绘制堆叠柱状图
    int barCount = visibleIds.size();
    double barWidth = double(chartWidth) / barCount * 0.7;
    double spacing = double(chartWidth) / barCount;

    int baseY = height() - marginBottom - legendHeight;

    for (int i = 0; i < barCount; ++i) {
        int classId = visibleIds[i];
        const QVector<int>& counts = m_scaleData[classId];
        int smallCount = counts[0];
        int mediumCount = counts[1];
        int largeCount = counts[2];
        int total = smallCount + mediumCount + largeCount;

        double x = marginLeft + spacing * i + (spacing - barWidth) / 2;

        // Small 段 (底部)
        double hSmall = double(smallCount) / maxTotal * chartHeight;
        double hMedium = double(mediumCount) / maxTotal * chartHeight;
        double hLarge = double(largeCount) / maxTotal * chartHeight;

        double yOffset = baseY;

        // Small
        if (smallCount > 0) {
            p.setBrush(colorSmall);
            p.setPen(Qt::NoPen);
            p.drawRect(QRectF(x, yOffset - hSmall, barWidth, hSmall));
            // 段内数量
            if (hSmall > 14) {
                p.setPen(Qt::white);
                p.setFont(QFont("Segoe UI", 7));
                p.drawText(QRectF(x, yOffset - hSmall, barWidth, hSmall),
                           Qt::AlignCenter, QString::number(smallCount));
            }
            yOffset -= hSmall;
        }

        // Medium
        if (mediumCount > 0) {
            p.setBrush(colorMedium);
            p.setPen(Qt::NoPen);
            p.drawRect(QRectF(x, yOffset - hMedium, barWidth, hMedium));
            if (hMedium > 14) {
                p.setPen(Qt::white);
                p.setFont(QFont("Segoe UI", 7));
                p.drawText(QRectF(x, yOffset - hMedium, barWidth, hMedium),
                           Qt::AlignCenter, QString::number(mediumCount));
            }
            yOffset -= hMedium;
        }

        // Large
        if (largeCount > 0) {
            p.setBrush(colorLarge);
            p.setPen(Qt::NoPen);
            p.drawRect(QRectF(x, yOffset - hLarge, barWidth, hLarge));
            if (hLarge > 14) {
                p.setPen(Qt::white);
                p.setFont(QFont("Segoe UI", 7));
                p.drawText(QRectF(x, yOffset - hLarge, barWidth, hLarge),
                           Qt::AlignCenter, QString::number(largeCount));
            }
            yOffset -= hLarge;
        }

        // 柱顶总数
        p.setPen(Qt::black);
        p.setFont(QFont("Segoe UI", 8));
        double topY = baseY - hSmall - hMedium - hLarge;
        p.drawText(QRectF(x, topY - 18, barWidth, 16), Qt::AlignCenter, QString::number(total));

        // X轴类别名
        QString label = (classId < m_classNames.size()) ? m_classNames[classId] : QString::number(classId);
        if (label.length() > 6) label = label.left(5) + "..";
        p.drawText(QRectF(x - 5, baseY + 5, barWidth + 10, 40),
                   Qt::AlignHCenter | Qt::AlignTop, label);
    }

    // Y轴标题
    p.save();
    p.translate(15, marginTop + chartHeight / 2);
    p.rotate(-90);
    p.drawText(QRect(-50, -10, 100, 20), Qt::AlignCenter, "instances");
    p.restore();

    // 图例
    p.setFont(QFont("Segoe UI", 9));
    int legendY = height() - legendHeight;
    int legendX = marginLeft + 10;
    int boxSize = 12;
    int legendSpacing = 20;

    auto drawLegend = [&](const QColor& color, const QString& text) {
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawRect(legendX, legendY + 4, boxSize, boxSize);
        p.setPen(Qt::black);
        QRect textRect(legendX + boxSize + 4, legendY, 120, legendHeight);
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
        legendX += boxSize + 4 + p.fontMetrics().horizontalAdvance(text) + legendSpacing;
    };

    drawLegend(colorSmall, "Small (P3)");
    drawLegend(colorMedium, "Medium (P4)");
    drawLegend(colorLarge, "Large (P5)");
}

// ==================== DatasetAnalysisDialog ====================

DatasetAnalysisDialog::DatasetAnalysisDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Dataset Analysis");
    setMinimumSize(900, 700);
    setupUI();
}

void DatasetAnalysisDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // 标题
    m_statusLabel = new QLabel("Loading...");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);

    // Tab Widget
    m_tabWidget = new QTabWidget();
    mainLayout->addWidget(m_tabWidget, 1);

    // ===== Tab 1: 总览 (Overview) =====
    auto* overviewTab = new QWidget();
    auto* overviewLayout = new QVBoxLayout(overviewTab);

    auto* gridLayout = new QGridLayout();
    gridLayout->setSpacing(10);

    // 左上：柱状图
    auto* barGroup = new QGroupBox("Class Distribution");
    auto* barLayout = new QVBoxLayout(barGroup);
    m_barChart = new BarChartWidget();
    barLayout->addWidget(m_barChart);
    gridLayout->addWidget(barGroup, 0, 0);

    // 右上：边界框叠加
    auto* boxGroup = new QGroupBox("Bounding Box Overlay");
    auto* boxLayout = new QVBoxLayout(boxGroup);
    m_boxOverlay = new BoxOverlayWidget();
    boxLayout->addWidget(m_boxOverlay);
    gridLayout->addWidget(boxGroup, 0, 1);

    // 左下：中心点热力图
    auto* centerGroup = new QGroupBox("Center Distribution");
    auto* centerLayout = new QVBoxLayout(centerGroup);
    m_centerHeatmap = new HeatmapWidget();
    m_centerHeatmap->setAxisLabels("x", "y");
    centerLayout->addWidget(m_centerHeatmap);
    gridLayout->addWidget(centerGroup, 1, 0);

    // 右下：尺寸热力图
    auto* sizeGroup = new QGroupBox("Size Distribution");
    auto* sizeLayout = new QVBoxLayout(sizeGroup);
    m_sizeHeatmap = new HeatmapWidget();
    m_sizeHeatmap->setAxisLabels("width", "height");
    sizeLayout->addWidget(m_sizeHeatmap);
    gridLayout->addWidget(sizeGroup, 1, 1);

    overviewLayout->addLayout(gridLayout);
    m_tabWidget->addTab(overviewTab, "总览 (Overview)");

    // ===== Tab 2: 尺度分布 (Scale) =====
    auto* scaleTab = new QWidget();
    auto* scaleLayout = new QVBoxLayout(scaleTab);

    // 复选框面板
    auto* filterLayout = new QHBoxLayout();
    auto* selectAllBtn = new QPushButton("全选");
    auto* clearAllBtn = new QPushButton("清除");
    selectAllBtn->setFixedWidth(60);
    clearAllBtn->setFixedWidth(60);
    connect(selectAllBtn, &QPushButton::clicked, this, &DatasetAnalysisDialog::onSelectAll);
    connect(clearAllBtn, &QPushButton::clicked, this, &DatasetAnalysisDialog::onClearAll);
    filterLayout->addWidget(selectAllBtn);
    filterLayout->addWidget(clearAllBtn);
    filterLayout->addStretch();
    scaleLayout->addLayout(filterLayout);

    // 复选框容器 (类别复选框将在 analyze() 中动态创建)
    m_classCheckBoxes.clear();

    // 堆叠柱状图
    auto* scaleGroup = new QGroupBox("Scale Distribution");
    auto* scaleGroupLayout = new QVBoxLayout(scaleGroup);
    m_scaleChart = new ScaleDistributionWidget();
    scaleGroupLayout->addWidget(m_scaleChart);
    scaleLayout->addWidget(scaleGroup, 1);

    m_tabWidget->addTab(scaleTab, "尺度分布 (Scale)");

    // 底部按钮
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeBtn);
    mainLayout->addLayout(buttonLayout);
}

void DatasetAnalysisDialog::setLabelsPath(const QString& path) {
    m_labelsPath = path;
}

void DatasetAnalysisDialog::setClassNames(const QStringList& names) {
    m_classNames = names;
}

void DatasetAnalysisDialog::analyze() {
    m_statusLabel->setText("Analyzing...");
    QApplication::processEvents();

    loadAllAnnotations();
    updateCharts();

    int totalAnnotations = 0;
    for (int count : m_classCount) {
        totalAnnotations += count;
    }

    m_statusLabel->setText(QString("Total: %1 annotations | %2 images | %3 classes")
                           .arg(totalAnnotations)
                           .arg(m_imageCount)
                           .arg(m_classCount.size()));
}

void DatasetAnalysisDialog::loadAllAnnotations() {
    m_classCount.clear();
    m_centers.clear();
    m_sizes.clear();
    m_boxes.clear();
    m_boxClassIds.clear();
    m_scaleDistribution.clear();
    m_imageCount = 0;

    QDir labelsDir(m_labelsPath);
    if (!labelsDir.exists()) return;

    QStringList txtFiles = labelsDir.entryList({"*.txt"}, QDir::Files);
    m_imageCount = txtFiles.size();

    AnnotationIO io;
    for (const QString& file : txtFiles) {
        auto annotations = io.load(labelsDir.filePath(file));

        for (const Annotation& ann : annotations) {
            int classId = ann.classId();
            m_classCount[classId]++;

            const BoundingBox& box = ann.boundingBox();
            m_centers.append(QPointF(box.centerX(), box.centerY()));
            m_sizes.append(QPointF(box.width(), box.height()));
            m_boxes.append(QRectF(box.left(), box.top(), box.width(), box.height()));
            m_boxClassIds.append(classId);

            // 尺度分类
            if (!m_scaleDistribution.contains(classId))
                m_scaleDistribution[classId] = QVector<int>(3, 0);

            double scale = std::sqrt(box.width() * box.height());
            if (scale < 0.05)
                m_scaleDistribution[classId][0]++; // Small
            else if (scale < 0.15)
                m_scaleDistribution[classId][1]++; // Medium
            else
                m_scaleDistribution[classId][2]++; // Large
        }
    }
}

void DatasetAnalysisDialog::updateCharts() {
    m_barChart->setData(m_classCount, m_classNames);
    m_centerHeatmap->setPoints(m_centers);
    m_sizeHeatmap->setPoints(m_sizes);
    m_boxOverlay->setBoxes(m_boxes, m_boxClassIds, m_classNames);

    // 构建类别复选框
    // 清除旧复选框
    for (QCheckBox* cb : m_classCheckBoxes)
        cb->deleteLater();
    m_classCheckBoxes.clear();

    // 找到 Scale tab 中 filterLayout 的下方插入复选框
    auto* scaleTab = m_tabWidget->widget(1);
    auto* scaleLayout = qobject_cast<QVBoxLayout*>(scaleTab->layout());

    // 创建复选框流式布局
    auto* checkboxWidget = new QWidget();
    auto* checkboxLayout = new QHBoxLayout(checkboxWidget);
    checkboxLayout->setContentsMargins(0, 0, 0, 0);
    checkboxLayout->setSpacing(8);

    QList<int> classIds = m_scaleDistribution.keys();
    for (int classId : classIds) {
        QString label = (classId < m_classNames.size()) ? m_classNames[classId] : QString::number(classId);
        auto* cb = new QCheckBox(label, checkboxWidget);
        cb->setChecked(true);
        connect(cb, &QCheckBox::toggled, this, &DatasetAnalysisDialog::onClassFilterChanged);
        checkboxLayout->addWidget(cb);
        m_classCheckBoxes.append(cb);
    }
    checkboxLayout->addStretch();

    // 插入在 filterLayout 和 scaleGroup 之间 (index 1)
    scaleLayout->insertWidget(1, checkboxWidget);

    // 初始化尺度图表
    onClassFilterChanged();
}

void DatasetAnalysisDialog::onClassFilterChanged() {
    QSet<int> visibleClasses;
    QList<int> classIds = m_scaleDistribution.keys();
    for (int i = 0; i < m_classCheckBoxes.size() && i < classIds.size(); ++i) {
        if (m_classCheckBoxes[i]->isChecked())
            visibleClasses.insert(classIds[i]);
    }
    m_scaleChart->setData(m_scaleDistribution, m_classNames, visibleClasses);
}

void DatasetAnalysisDialog::onSelectAll() {
    for (QCheckBox* cb : m_classCheckBoxes)
        cb->setChecked(true);
}

void DatasetAnalysisDialog::onClearAll() {
    for (QCheckBox* cb : m_classCheckBoxes)
        cb->setChecked(false);
}
