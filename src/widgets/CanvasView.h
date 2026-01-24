#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include "core/CoordinateTransform.h"
#include "core/Annotation.h"
#include "core/SkeletonConfig.h"

class AnnotationItem;

/**
 * @brief 编辑模式
 */
enum class EditMode {
    Select,         // 选择模式
    DrawBBox,       // 绘制边界框
    DrawKeypoint,   // 添加关键点
    Pan             // 平移模式
};

/**
 * @brief 图像画布视图
 */
class CanvasView : public QGraphicsView {
    Q_OBJECT
public:
    explicit CanvasView(QWidget* parent = nullptr);
    ~CanvasView();

    // 设置图像
    void setImage(const QImage& image);
    void setImage(const QString& imagePath);
    QImage currentImage() const { return m_image; }
    QString currentImagePath() const { return m_imagePath; }

    // 设置标注
    void setAnnotations(const QVector<Annotation>& annotations);
    QVector<Annotation> annotations() const { return m_annotations; }
    void addAnnotation(const Annotation& ann);
    void removeAnnotation(int index);
    void updateAnnotation(int index, const Annotation& ann);
    void clearAnnotations();

    // 编辑模式
    void setEditMode(EditMode mode);
    EditMode editMode() const { return m_mode; }

    // 当前选中
    void setSelectedAnnotation(int index);
    int selectedAnnotation() const { return m_selectedIndex; }
    void clearSelection();

    // 类别设置
    void setCurrentClass(int classId) { m_currentClass = classId; }
    int currentClass() const { return m_currentClass; }

    // 骨架配置
    void setSkeletonConfig(const SkeletonConfig& config);
    const SkeletonConfig& skeletonConfig() const { return m_skeleton; }

    // 类别名称
    void setClassNames(const QStringList& names) { m_classNames = names; }

    // 坐标变换
    const CoordinateTransform& coordTransform() const { return m_transform; }

    // 显示设置
    void setCrossHairEnabled(bool enabled);
    bool crossHairEnabled() const { return m_crossHairEnabled; }
    void setCrossHairColor(const QColor& color);
    QColor crossHairColor() const { return m_crossHairColor; }

    void setAnnotationColorEnabled(bool enabled);
    bool annotationColorEnabled() const { return m_annotationColorEnabled; }
    void setAnnotationColor(const QColor& color);
    QColor annotationColor() const { return m_annotationColor; }

    // 缩放
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomOriginal();
    double currentZoom() const { return m_transform.scale(); }

signals:
    void annotationsChanged();
    void annotationSelected(int index);
    void annotationDoubleClicked(int index);
    void zoomChanged(double scale);
    void mouseMoved(const QPointF& imagePos);
    void classSelectRequested(int annotationIndex);  // 请求选择类别
    void contextMenuRequested(int annotationIndex, const QPoint& globalPos);  // 右键菜单

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void enterEvent(QEnterEvent* event) override;

private:
    QGraphicsScene* m_scene;
    QGraphicsPixmapItem* m_pixmapItem = nullptr;
    QImage m_image;
    QString m_imagePath;

    CoordinateTransform m_transform;
    SkeletonConfig m_skeleton;
    QStringList m_classNames;
    EditMode m_mode = EditMode::Select;
    int m_currentClass = 0;

    // 标注数据
    QVector<Annotation> m_annotations;
    QVector<QGraphicsItem*> m_annotationItems;
    int m_selectedIndex = -1;

    // 绘制状态
    bool m_drawing = false;
    QPointF m_drawStart;
    QGraphicsRectItem* m_tempRect = nullptr;

    // 平移状态
    bool m_panning = false;
    QPoint m_panStart;

    // 拖动状态
    bool m_dragging = false;
    int m_dragAnnIndex = -1;
    int m_dragPointIndex = -1;  // -1=整体拖动, 0-3=边框角点, >=4=关键点
    QPointF m_dragStartPos;

    // 十字辅助线
    QGraphicsLineItem* m_crossHairH = nullptr;
    QGraphicsLineItem* m_crossHairV = nullptr;
    bool m_crossHairEnabled = true;
    QColor m_crossHairColor = QColor(0, 255, 0, 180);

    // 标注框颜色设置
    bool m_annotationColorEnabled = false;
    QColor m_annotationColor = Qt::green;

    void updateScene();
    void updateAnnotationItems();
    void updateTransform();
    void updateCrossHair(const QPointF& scenePos);
    void finishDrawBBox(const QPointF& end);
    int findAnnotationAt(const QPointF& scenePos);
    int findPointAt(int annIndex, const QPointF& scenePos);
    void applyZoom(double factor, const QPointF& center);
};
