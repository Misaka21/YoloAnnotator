#pragma once
#include <QGraphicsItemGroup>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include "core/Annotation.h"
#include "core/SkeletonConfig.h"
#include "core/CoordinateTransform.h"

/**
 * @brief 标注图形项，显示边界框、关键点和骨架
 */
class AnnotationItem : public QGraphicsItemGroup {
public:
    AnnotationItem(const Annotation& annotation,
                   const CoordinateTransform& transform,
                   const SkeletonConfig& skeleton,
                   const QString& className,
                   QGraphicsItem* parent = nullptr);
    ~AnnotationItem();

    // 更新显示
    void updateDisplay();

    // 选中状态
    void setSelected(bool selected);
    bool isSelected() const { return m_isSelected; }

    // 标注索引
    int annotationIndex() const { return m_annotationIndex; }
    void setAnnotationIndex(int index) { m_annotationIndex = index; }

private:
    const Annotation& m_annotation;
    const CoordinateTransform& m_transform;
    const SkeletonConfig& m_skeleton;
    QString m_className;

    QGraphicsRectItem* m_bboxItem = nullptr;
    QGraphicsTextItem* m_labelItem = nullptr;
    QVector<QGraphicsEllipseItem*> m_keypointItems;
    QVector<QGraphicsLineItem*> m_boneItems;
    QVector<QGraphicsEllipseItem*> m_cornerItems;

    int m_annotationIndex = -1;
    bool m_isSelected = false;

    void createBBoxItem();
    void createKeypointItems();
    void createBoneItems();
    void createCornerItems();
    void updatePositions();
};
