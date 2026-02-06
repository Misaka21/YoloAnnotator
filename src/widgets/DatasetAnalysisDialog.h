#pragma once
#include <QDialog>
#include <QMap>
#include <QSet>
#include <QVector>
#include <QPointF>
#include <QRectF>

class QLabel;
class QTabWidget;
class QCheckBox;

/**
 * @brief 柱状图绘制 Widget
 */
class BarChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit BarChartWidget(QWidget* parent = nullptr);

    void setData(const QMap<int, int>& classCount, const QStringList& classNames);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QMap<int, int> m_classCount;
    QStringList m_classNames;
};

/**
 * @brief 热力图绘制 Widget
 */
class HeatmapWidget : public QWidget {
    Q_OBJECT
public:
    explicit HeatmapWidget(QWidget* parent = nullptr);

    void setPoints(const QVector<QPointF>& points);
    void setAxisLabels(const QString& xLabel, const QString& yLabel);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<QPointF> m_points;
    QString m_xLabel = "x";
    QString m_yLabel = "y";
    static const int GRID_SIZE = 50;
};

/**
 * @brief 边界框叠加绘制 Widget
 */
class BoxOverlayWidget : public QWidget {
    Q_OBJECT
public:
    explicit BoxOverlayWidget(QWidget* parent = nullptr);

    void setBoxes(const QVector<QRectF>& boxes, const QVector<int>& classIds,
                  const QStringList& classNames);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<QRectF> m_boxes;
    QVector<int> m_classIds;
    QStringList m_classNames;

    QColor getColorForClass(int classId, const QString& className) const;
};

/**
 * @brief 堆叠柱状图 Widget（尺度分布）
 */
class ScaleDistributionWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScaleDistributionWidget(QWidget* parent = nullptr);

    // scaleData: classId → [smallCount, mediumCount, largeCount]
    void setData(const QMap<int, QVector<int>>& scaleData,
                 const QStringList& classNames,
                 const QSet<int>& visibleClasses);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QMap<int, QVector<int>> m_scaleData;
    QStringList m_classNames;
    QSet<int> m_visibleClasses;
};

/**
 * @brief 数据集分析对话框
 */
class DatasetAnalysisDialog : public QDialog {
    Q_OBJECT
public:
    explicit DatasetAnalysisDialog(QWidget* parent = nullptr);

    void setLabelsPath(const QString& path);
    void setClassNames(const QStringList& names);
    void analyze();

private:
    QString m_labelsPath;
    QStringList m_classNames;

    // 统计数据
    QMap<int, int> m_classCount;
    QVector<QPointF> m_centers;
    QVector<QPointF> m_sizes;  // 用 QPointF 存储 (width, height)
    QVector<QRectF> m_boxes;
    QVector<int> m_boxClassIds;
    int m_imageCount = 0;
    QMap<int, QVector<int>> m_scaleDistribution; // classId → [small, medium, large]

    // UI 组件
    QTabWidget* m_tabWidget;
    BarChartWidget* m_barChart;
    HeatmapWidget* m_centerHeatmap;
    HeatmapWidget* m_sizeHeatmap;
    BoxOverlayWidget* m_boxOverlay;
    ScaleDistributionWidget* m_scaleChart;
    QVector<QCheckBox*> m_classCheckBoxes;
    QLabel* m_statusLabel;

    void setupUI();
    void loadAllAnnotations();
    void updateCharts();
    void onClassFilterChanged();
    void onSelectAll();
    void onClearAll();
};
