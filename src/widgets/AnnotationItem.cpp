#include "AnnotationItem.h"
#include <QFont>
#include <QPen>
#include <QBrush>

AnnotationItem::AnnotationItem(const Annotation& annotation,
                               const CoordinateTransform& transform,
                               const SkeletonConfig& skeleton,
                               const QString& className,
                               QGraphicsItem* parent)
    : QGraphicsItemGroup(parent)
    , m_annotation(annotation)
    , m_transform(transform)
    , m_skeleton(skeleton)
    , m_className(className)
{
    createBBoxItem();
    createCornerItems();
    createKeypointItems();
    createBoneItems();
    updatePositions();
}

AnnotationItem::~AnnotationItem() = default;

void AnnotationItem::createBBoxItem() {
    m_bboxItem = new QGraphicsRectItem(this);
    m_bboxItem->setPen(QPen(Qt::green, 2));
    m_bboxItem->setBrush(Qt::NoBrush);
    addToGroup(m_bboxItem);

    // 标签
    m_labelItem = new QGraphicsTextItem(this);
    m_labelItem->setDefaultTextColor(Qt::white);
    QFont font;
    font.setPointSize(10);
    font.setBold(true);
    m_labelItem->setFont(font);
    addToGroup(m_labelItem);
}

void AnnotationItem::createCornerItems() {
    // 4个角点
    for (int i = 0; i < 4; ++i) {
        auto* corner = new QGraphicsEllipseItem(-4, -4, 8, 8, this);
        corner->setPen(QPen(Qt::green, 1));
        corner->setBrush(QBrush(Qt::green));
        corner->setZValue(1);
        m_cornerItems.append(corner);
        addToGroup(corner);
    }
}

void AnnotationItem::createKeypointItems() {
    if (m_skeleton.keypointCount() == 0) return;

    for (int i = 0; i < m_annotation.keypointCount(); ++i) {
        QColor color = m_skeleton.keypointColor(i);
        auto* kpItem = new QGraphicsEllipseItem(-5, -5, 10, 10, this);
        kpItem->setPen(QPen(color.darker(), 1));
        kpItem->setBrush(QBrush(color));
        kpItem->setZValue(2);
        m_keypointItems.append(kpItem);
        addToGroup(kpItem);
    }
}

void AnnotationItem::createBoneItems() {
    for (const auto& bone : m_skeleton.bones()) {
        auto* line = new QGraphicsLineItem(this);
        line->setPen(QPen(bone.color, 2));
        line->setZValue(1);
        m_boneItems.append(line);
        addToGroup(line);
    }
}

void AnnotationItem::updateDisplay() {
    updatePositions();
}

void AnnotationItem::setSelected(bool selected) {
    m_isSelected = selected;

    QColor bboxColor = selected ? Qt::red : Qt::green;
    int bboxWidth = selected ? 3 : 2;

    m_bboxItem->setPen(QPen(bboxColor, bboxWidth));

    for (auto* corner : m_cornerItems) {
        corner->setPen(QPen(bboxColor, 1));
        corner->setBrush(QBrush(bboxColor));
        corner->setVisible(selected);
    }
}

void AnnotationItem::updatePositions() {
    const BoundingBox& bbox = m_annotation.boundingBox();

    // 边界框位置
    QRectF viewRect = m_transform.normalizedToView(bbox.toRect());
    m_bboxItem->setRect(viewRect);

    // 标签位置
    QString label = QString("%1 [%2]").arg(m_className).arg(m_annotation.classId());
    m_labelItem->setPlainText(label);
    m_labelItem->setPos(viewRect.topLeft() + QPointF(2, -20));

    // 角点位置
    QPointF corners[4] = {
        viewRect.topLeft(),
        viewRect.topRight(),
        viewRect.bottomRight(),
        viewRect.bottomLeft()
    };
    for (int i = 0; i < 4 && i < m_cornerItems.size(); ++i) {
        m_cornerItems[i]->setPos(corners[i]);
    }

    // 关键点位置
    for (int i = 0; i < m_keypointItems.size() && i < m_annotation.keypointCount(); ++i) {
        const Keypoint& kp = m_annotation.keypoints()[i];
        if (kp.isValid()) {
            QPointF viewPos = m_transform.normalizedToView(kp.position());
            m_keypointItems[i]->setPos(viewPos);
            m_keypointItems[i]->setVisible(true);

            // 根据可见性调整透明度
            int alpha = (kp.visibility() == 0) ? 50 :
                        (kp.visibility() == 1) ? 150 : 255;
            QColor color = m_skeleton.keypointColor(i);
            color.setAlpha(alpha);
            m_keypointItems[i]->setBrush(QBrush(color));
        } else {
            m_keypointItems[i]->setVisible(false);
        }
    }

    // 骨架连线位置
    const auto& bones = m_skeleton.bones();
    for (int i = 0; i < m_boneItems.size() && i < bones.size(); ++i) {
        int from = bones[i].from;
        int to = bones[i].to;

        if (from < m_annotation.keypointCount() && to < m_annotation.keypointCount()) {
            const Keypoint& kp1 = m_annotation.keypoints()[from];
            const Keypoint& kp2 = m_annotation.keypoints()[to];

            if (kp1.isValid() && kp2.isValid()) {
                QPointF p1 = m_transform.normalizedToView(kp1.position());
                QPointF p2 = m_transform.normalizedToView(kp2.position());
                m_boneItems[i]->setLine(QLineF(p1, p2));
                m_boneItems[i]->setVisible(true);
            } else {
                m_boneItems[i]->setVisible(false);
            }
        } else {
            m_boneItems[i]->setVisible(false);
        }
    }
}
