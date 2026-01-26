#include "DatasetAnalysisDialog.h"
#include "io/AnnotationIO.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
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

    int chartWidth = width() - marginLeft - marginRight;
    int chartHeight = height() - marginTop - marginBottom;

    // 绘制背景
    p.fillRect(marginLeft, marginTop, chartWidth, chartHeight, Qt::white);

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

    // 绘制热力图（蓝色渐变）
    double cellW = double(chartWidth) / GRID_SIZE;
    double cellH = double(chartHeight) / GRID_SIZE;

    for (int gy = 0; gy < GRID_SIZE; ++gy) {
        for (int gx = 0; gx < GRID_SIZE; ++gx) {
            if (grid[gy][gx] > 0) {
                double intensity = double(grid[gy][gx]) / maxDensity;
                // 蓝色渐变：从浅蓝到深蓝
                int alpha = int(50 + 205 * intensity);
                QColor color(100, 150, 255, alpha);

                double x = marginLeft + gx * cellW;
                double y = marginTop + gy * cellH;
                p.fillRect(QRectF(x, y, cellW + 1, cellH + 1), color);
            }
        }
    }

    // 绘制边框
    p.setPen(Qt::black);
    p.setBrush(Qt::NoBrush);
    p.drawRect(marginLeft, marginTop, chartWidth, chartHeight);

    // 坐标轴标签
    p.setFont(QFont("Segoe UI", 8));

    // X轴
    for (int i = 0; i <= 4; ++i) {
        double val = i * 0.25;
        int x = marginLeft + chartWidth * i / 4;
        p.drawText(QRect(x - 20, height() - marginBottom + 5, 40, 15),
                   Qt::AlignCenter, QString::number(val, 'f', 1));
    }
    p.drawText(QRect(marginLeft, height() - 20, chartWidth, 20),
               Qt::AlignCenter, m_xLabel);

    // Y轴
    for (int i = 0; i <= 4; ++i) {
        double val = i * 0.25;
        int y = marginTop + chartHeight - chartHeight * i / 4;
        p.drawText(QRect(0, y - 8, marginLeft - 5, 16),
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(val, 'f', 1));
    }

    p.save();
    p.translate(12, marginTop + chartHeight / 2);
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

    // 绘制所有边界框
    for (int i = 0; i < m_boxes.size(); ++i) {
        const QRectF& box = m_boxes[i];
        int classId = (i < m_classIds.size()) ? m_classIds[i] : 0;
        QString className = (classId < m_classNames.size()) ? m_classNames[classId] : "";

        QColor color = getColorForClass(classId, className);

        // 转换到绘制坐标
        double x = offsetX + box.x() * chartSize;
        double y = offsetY + box.y() * chartSize;
        double w = box.width() * chartSize;
        double h = box.height() * chartSize;

        p.setPen(QPen(color.darker(120), 1));
        p.setBrush(QBrush(color));
        p.drawRect(QRectF(x, y, w, h));
    }
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

    // 2x2 网格布局
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

    mainLayout->addLayout(gridLayout, 1);

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
        }
    }
}

void DatasetAnalysisDialog::updateCharts() {
    m_barChart->setData(m_classCount, m_classNames);
    m_centerHeatmap->setPoints(m_centers);
    m_sizeHeatmap->setPoints(m_sizes);
    m_boxOverlay->setBoxes(m_boxes, m_boxClassIds, m_classNames);
}
