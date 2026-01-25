#include "CanvasView.h"
#include "AnnotationItem.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QContextMenuEvent>
#include <QTextDocument>
#include <cmath>

CanvasView::CanvasView(QWidget* parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setBackgroundBrush(QBrush(QColor(50, 50, 50)));
    setMouseTracking(true);

    // 创建十字辅助线
    QPen crossPen(m_crossHairColor, 1, Qt::DashLine);
    m_crossHairH = m_scene->addLine(0, 0, 0, 0, crossPen);
    m_crossHairV = m_scene->addLine(0, 0, 0, 0, crossPen);
    m_crossHairH->setZValue(1000);
    m_crossHairV->setZValue(1000);
    m_crossHairH->setVisible(false);
    m_crossHairV->setVisible(false);
}

CanvasView::~CanvasView() = default;

void CanvasView::setImage(const QImage& image) {
    m_image = image;
    m_imagePath.clear();
    m_transform.setImageSize(image.size());

    // 清空场景
    m_scene->clear();
    m_pixmapItem = nullptr;
    m_annotationItems.clear();

    // 重新创建十字辅助线
    QPen crossPen(m_crossHairColor, 1, Qt::DashLine);
    m_crossHairH = m_scene->addLine(0, 0, 0, 0, crossPen);
    m_crossHairV = m_scene->addLine(0, 0, 0, 0, crossPen);
    m_crossHairH->setZValue(1000);
    m_crossHairV->setZValue(1000);
    m_crossHairH->setVisible(m_mode == EditMode::DrawBBox && m_crossHairEnabled);
    m_crossHairV->setVisible(m_mode == EditMode::DrawBBox && m_crossHairEnabled);

    // 添加图片
    m_pixmapItem = m_scene->addPixmap(QPixmap::fromImage(image));
    m_pixmapItem->setZValue(-1);

    // 设置场景大小为图片大小
    m_scene->setSceneRect(0, 0, image.width(), image.height());

    zoomFit();
    updateAnnotationItems();
}

void CanvasView::setImage(const QString& imagePath) {
    QImage img(imagePath);
    if (!img.isNull()) {
        m_imagePath = imagePath;
        setImage(img);
    }
}

void CanvasView::setAnnotations(const QVector<Annotation>& annotations) {
    m_annotations = annotations;
    m_selectedIndex = -1;
    // 初始化历史
    m_history.clear();
    m_history.append(m_annotations);
    m_historyIndex = 0;
    updateAnnotationItems();
}

void CanvasView::addAnnotation(const Annotation& ann) {
    saveToHistory();
    m_annotations.append(ann);
    updateAnnotationItems();
    emit annotationsChanged();
}

void CanvasView::removeAnnotation(int index) {
    if (index >= 0 && index < m_annotations.size()) {
        saveToHistory();
        m_annotations.removeAt(index);
        if (m_selectedIndex == index) {
            m_selectedIndex = -1;
        } else if (m_selectedIndex > index) {
            m_selectedIndex--;
        }
        updateAnnotationItems();
        emit annotationsChanged();
    }
}

void CanvasView::updateAnnotation(int index, const Annotation& ann) {
    if (index >= 0 && index < m_annotations.size()) {
        saveToHistory();
        m_annotations[index] = ann;
        updateAnnotationItems();
        emit annotationsChanged();
    }
}

void CanvasView::clearAnnotations() {
    if (!m_annotations.isEmpty()) {
        saveToHistory();
    }
    m_annotations.clear();
    m_selectedIndex = -1;
    updateAnnotationItems();
    emit annotationsChanged();
}

void CanvasView::setEditMode(EditMode mode) {
    m_mode = mode;
    if (mode == EditMode::Pan) {
        setCursor(Qt::OpenHandCursor);
    } else if (mode == EditMode::DrawBBox || mode == EditMode::DrawKeypoint) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
    // 只在边界框模式下且启用时显示十字线
    bool showCross = (mode == EditMode::DrawBBox) && m_crossHairEnabled;
    m_crossHairH->setVisible(showCross);
    m_crossHairV->setVisible(showCross);
}

void CanvasView::setSelectedAnnotation(int index) {
    if (index != m_selectedIndex) {
        m_selectedIndex = index;
        updateAnnotationItems();
        emit annotationSelected(index);
    }
}

void CanvasView::clearSelection() {
    setSelectedAnnotation(-1);
}

void CanvasView::setSkeletonConfig(const SkeletonConfig& config) {
    m_skeleton = config;
    updateAnnotationItems();
}

void CanvasView::zoomIn() {
    scale(1.2, 1.2);
    emit zoomChanged(transform().m11());
    updateAnnotationItems();
}

void CanvasView::zoomOut() {
    scale(1.0 / 1.2, 1.0 / 1.2);
    emit zoomChanged(transform().m11());
    updateAnnotationItems();
}

void CanvasView::zoomFit() {
    if (m_image.isNull()) return;
    fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    emit zoomChanged(transform().m11());
    updateAnnotationItems();
}

void CanvasView::zoomOriginal() {
    resetTransform();
    emit zoomChanged(1.0);
    updateAnnotationItems();
}

void CanvasView::wheelEvent(QWheelEvent* event) {
    // 计算缩放因子
    double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);

    // 限制缩放范围
    double currentScale = transform().m11();
    double newScale = currentScale * factor;
    if (newScale < 0.1 || newScale > 50.0) {
        event->accept();
        return;
    }

    // 获取鼠标在场景中的位置（缩放前）
    QPointF targetScenePos = mapToScene(event->position().toPoint());

    // 执行缩放
    scale(factor, factor);

    // 获取同一场景点在缩放后的视口位置
    QPointF targetViewPos = mapFromScene(targetScenePos);

    // 计算需要调整的偏移量，使场景点回到鼠标位置
    QPointF delta = targetViewPos - event->position();

    // 调整滚动条
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + int(delta.x()));
    verticalScrollBar()->setValue(verticalScrollBar()->value() + int(delta.y()));

    emit zoomChanged(transform().m11());
    updateAnnotationItems();
    event->accept();
}

void CanvasView::mousePressEvent(QMouseEvent* event) {
    QPointF scenePos = mapToScene(event->pos());

    if (event->button() == Qt::LeftButton) {
        if (m_mode == EditMode::DrawBBox) {
            // 开始绘制边界框
            m_drawing = true;
            m_drawStart = scenePos;
            m_tempRect = m_scene->addRect(QRectF(scenePos, scenePos),
                                          QPen(Qt::green, 2), QBrush(QColor(0, 255, 0, 50)));
            m_tempRect->setZValue(100);
        } else if (m_mode == EditMode::Select) {
            // 优先检查当前选中标注的角点/关键点
            if (m_selectedIndex >= 0 && m_selectedIndex < m_annotations.size()) {
                int ptIdx = findPointAt(m_selectedIndex, scenePos);
                if (ptIdx >= 0) {
                    // 点击在当前选中框的控制点上，开始拖动
                    m_dragging = true;
                    m_dragAnnIndex = m_selectedIndex;
                    m_dragPointIndex = ptIdx;
                    m_dragStartPos = scenePos;
                    saveToHistory();
                    return;
                }
            }

            // 否则尝试选择其他标注
            int annIdx = findAnnotationAt(scenePos);
            if (annIdx >= 0) {
                setSelectedAnnotation(annIdx);
                int ptIdx = findPointAt(annIdx, scenePos);
                m_dragging = true;
                m_dragAnnIndex = annIdx;
                m_dragPointIndex = ptIdx;  // -1 表示整体拖动
                m_dragStartPos = scenePos;
                saveToHistory();
            } else {
                clearSelection();
            }
        } else if (m_mode == EditMode::DrawKeypoint) {
            // 添加关键点到选中的标注
            if (m_selectedIndex >= 0 && m_selectedIndex < m_annotations.size()) {
                saveToHistory();  // 添加关键点前保存历史
                Annotation& ann = m_annotations[m_selectedIndex];
                // 转换为归一化坐标
                double nx = scenePos.x() / m_image.width();
                double ny = scenePos.y() / m_image.height();
                // 找到第一个无效的关键点位置
                for (int i = 0; i < ann.keypointCount(); ++i) {
                    if (!ann.keypoints()[i].isValid()) {
                        Keypoint kp(nx, ny, 2);
                        kp.setValid(true);
                        ann.setKeypoint(i, kp);
                        updateAnnotationItems();
                        emit annotationsChanged();
                        break;
                    }
                }
            }
        }
    } else if (event->button() == Qt::RightButton) {
        // 右键平移
        m_panning = true;
        m_panStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
    } else if (event->button() == Qt::MiddleButton) {
        // 中键拖动整个标注
        int annIdx = findAnnotationAt(scenePos);
        if (annIdx >= 0) {
            setSelectedAnnotation(annIdx);
            m_dragging = true;
            m_dragAnnIndex = annIdx;
            m_dragPointIndex = -1;  // 整体拖动
            m_dragStartPos = scenePos;
            saveToHistory();  // 拖动前保存历史
        }
    }
}

void CanvasView::mouseMoveEvent(QMouseEvent* event) {
    QPointF scenePos = mapToScene(event->pos());
    emit mouseMoved(scenePos);

    // 更新十字辅助线
    if (m_mode == EditMode::DrawBBox) {
        updateCrossHair(scenePos);
    }

    if (m_drawing && m_tempRect) {
        // 更新临时矩形
        QRectF rect = QRectF(m_drawStart, scenePos).normalized();
        m_tempRect->setRect(rect);
    } else if (m_panning) {
        // 平移视图
        QPointF delta = event->pos() - m_panStart;
        m_panStart = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    } else if (m_dragging && m_dragAnnIndex >= 0) {
        // 拖动标注或点
        double imgW = m_image.width();
        double imgH = m_image.height();

        // 场景坐标转归一化坐标
        double nx = scenePos.x() / imgW;
        double ny = scenePos.y() / imgH;
        double sx = m_dragStartPos.x() / imgW;
        double sy = m_dragStartPos.y() / imgH;
        double dx = nx - sx;
        double dy = ny - sy;

        Annotation& ann = m_annotations[m_dragAnnIndex];

        if (m_dragPointIndex == -1) {
            // 整体移动边界框
            BoundingBox& bbox = ann.boundingBox();
            bbox.setCenterX(bbox.centerX() + dx);
            bbox.setCenterY(bbox.centerY() + dy);
            bbox.clamp();

            // 同时移动关键点
            for (Keypoint& kp : ann.keypoints()) {
                if (kp.isValid()) {
                    kp.setX(kp.x() + dx);
                    kp.setY(kp.y() + dy);
                }
            }
        } else if (m_dragPointIndex >= 0 && m_dragPointIndex < 4) {
            // 拖动边界框角点
            BoundingBox& bbox = ann.boundingBox();
            double l = bbox.left(), r = bbox.right();
            double t = bbox.top(), b = bbox.bottom();

            switch (m_dragPointIndex) {
                case 0: l = nx; t = ny; break;  // 左上
                case 1: r = nx; t = ny; break;  // 右上
                case 2: r = nx; b = ny; break;  // 右下
                case 3: l = nx; b = ny; break;  // 左下
            }

            if (r > l && b > t) {
                bbox.setRect((l + r) / 2, (t + b) / 2, r - l, b - t);
            }
        } else {
            // 拖动关键点
            int kpIdx = m_dragPointIndex - 4;
            if (kpIdx >= 0 && kpIdx < ann.keypointCount()) {
                Keypoint& kp = ann.keypoints()[kpIdx];
                kp.setPosition(nx, ny);
            }
        }

        m_dragStartPos = scenePos;
        updateAnnotationItems();
    }
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_drawing) {
            QPointF endPos = mapToScene(event->pos());
            finishDrawBBox(endPos);
            m_drawing = false;
        }
        if (m_dragging) {
            m_dragging = false;
            emit annotationsChanged();
        }
    } else if (event->button() == Qt::RightButton) {
        m_panning = false;
        setCursor(m_mode == EditMode::Pan ? Qt::OpenHandCursor : Qt::ArrowCursor);
    } else if (event->button() == Qt::MiddleButton) {
        if (m_dragging) {
            m_dragging = false;
            emit annotationsChanged();
        }
    }
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPointF scenePos = mapToScene(event->pos());
        int annIdx = findAnnotationAt(scenePos);
        if (annIdx >= 0) {
            emit annotationDoubleClicked(annIdx);
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void CanvasView::contextMenuEvent(QContextMenuEvent* event) {
    QPointF scenePos = mapToScene(event->pos());
    int annIdx = findAnnotationAt(scenePos);
    if (annIdx >= 0) {
        setSelectedAnnotation(annIdx);
        emit contextMenuRequested(annIdx, event->globalPos());
        event->accept();
    } else {
        QGraphicsView::contextMenuEvent(event);
    }
}

void CanvasView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (m_selectedIndex >= 0) {
            removeAnnotation(m_selectedIndex);
        }
    } else if (event->key() == Qt::Key_Escape) {
        clearSelection();
        if (m_drawing && m_tempRect) {
            m_scene->removeItem(m_tempRect);
            delete m_tempRect;
            m_tempRect = nullptr;
            m_drawing = false;
        }
    }
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
}

void CanvasView::updateScene() {
    updateAnnotationItems();
}

void CanvasView::updateAnnotationItems() {
    // 清除旧的标注项（但保留图片）
    for (auto* item : m_annotationItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_annotationItems.clear();

    if (m_image.isNull()) return;

    double imgW = m_image.width();
    double imgH = m_image.height();

    // 创建新的标注项
    for (int i = 0; i < m_annotations.size(); ++i) {
        const Annotation& ann = m_annotations[i];
        QString className;
        if (ann.classId() < m_classNames.size()) {
            className = m_classNames[ann.classId()];
        } else {
            className = QString::number(ann.classId());
        }

        // 重新计算位置（使用像素坐标）
        const BoundingBox& bbox = ann.boundingBox();
        double x = bbox.left() * imgW;
        double y = bbox.top() * imgH;
        double w = bbox.width() * imgW;
        double h = bbox.height() * imgH;

        // 创建简单的矩形和文字
        QColor color;
        if (i == m_selectedIndex) {
            color = Qt::red;  // 选中始终红色
        } else {
            // 根据类别名称前缀设置颜色
            QString lowerName = className.toLower();
            if (lowerName.startsWith("r-") || lowerName.startsWith("r_")) {
                color = Qt::red;
            } else if (lowerName.startsWith("b-") || lowerName.startsWith("b_")) {
                color = Qt::blue;
            } else if (lowerName.startsWith("w-") || lowerName.startsWith("w_") || lowerName.startsWith("w")) {
                color = Qt::white;
            } else if (lowerName.startsWith("p-") || lowerName.startsWith("p_") || lowerName.startsWith("p")) {
                color = QColor(255, 105, 180);  // 粉色
            } else {
                color = Qt::green;  // 默认绿色
            }
        }
        double scale = transform().m11();
        double penWidth = ((i == m_selectedIndex) ? 3.0 : 2.0) / scale;  // 固定屏幕宽度
        QColor fillColor = color;
        fillColor.setAlpha(40);  // 淡淡的填充

        auto* rectItem = m_scene->addRect(x, y, w, h, QPen(color, penWidth), QBrush(fillColor));
        rectItem->setZValue(1);
        m_annotationItems.append(rectItem);

        // 添加标签文字
        QString labelText = QString("[%1] %2").arg(i).arg(className);

        // 字体设置
        QFont labelFont("Segoe UI", 9);
        labelFont.setWeight(QFont::DemiBold);
        labelFont.setStyleStrategy(QFont::PreferAntialias);

        // 先创建文字获取尺寸
        auto* textItem = m_scene->addText(labelText, labelFont);
        textItem->document()->setDocumentMargin(0);  // 去掉内边距
        QRectF textRect = textItem->boundingRect();
        double labelX = x;
        double labelY = y - textRect.height();

        // 阴影文字
        auto* shadowItem = m_scene->addText(labelText, labelFont);
        shadowItem->document()->setDocumentMargin(0);
        shadowItem->setPos(labelX + 1, labelY + 1);
        shadowItem->setDefaultTextColor(QColor(0, 0, 0, 200));
        shadowItem->setZValue(2);
        m_annotationItems.append(shadowItem);

        // 主文字
        textItem->setPos(labelX, labelY);
        textItem->setDefaultTextColor(Qt::white);
        textItem->setZValue(3);
        m_annotationItems.append(textItem);

        // 如果选中，绘制角点 (固定屏幕大小，不随缩放变化)
        if (i == m_selectedIndex) {
            double scale = transform().m11();
            double cornerRadius = 8.0 / scale;  // 屏幕上固定8像素半径

            QPointF corners[4] = {
                {x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}
            };
            for (int j = 0; j < 4; ++j) {
                auto* corner = m_scene->addEllipse(
                    corners[j].x() - cornerRadius, corners[j].y() - cornerRadius,
                    cornerRadius * 2, cornerRadius * 2,
                    QPen(color), QBrush(color));
                corner->setZValue(3);
                m_annotationItems.append(corner);
            }
        }

        // 绘制关键点
        for (int k = 0; k < ann.keypointCount(); ++k) {
            const Keypoint& kp = ann.keypoints()[k];
            if (kp.isValid()) {
                double kx = kp.x() * imgW;
                double ky = kp.y() * imgH;
                QColor kpColor = m_skeleton.keypointColor(k);
                auto* kpItem = m_scene->addEllipse(
                    kx - 5, ky - 5, 10, 10,
                    QPen(kpColor.darker()), QBrush(kpColor));
                kpItem->setZValue(3);
                m_annotationItems.append(kpItem);
            }
        }

        // 绘制骨架连线
        for (const auto& bone : m_skeleton.bones()) {
            if (bone.from < ann.keypointCount() && bone.to < ann.keypointCount()) {
                const Keypoint& kp1 = ann.keypoints()[bone.from];
                const Keypoint& kp2 = ann.keypoints()[bone.to];
                if (kp1.isValid() && kp2.isValid()) {
                    double x1 = kp1.x() * imgW;
                    double y1 = kp1.y() * imgH;
                    double x2 = kp2.x() * imgW;
                    double y2 = kp2.y() * imgH;
                    auto* lineItem = m_scene->addLine(x1, y1, x2, y2, QPen(bone.color, 2));
                    lineItem->setZValue(2);
                    m_annotationItems.append(lineItem);
                }
            }
        }
    }
}

void CanvasView::updateTransform() {
    emit zoomChanged(transform().m11());
}

void CanvasView::finishDrawBBox(const QPointF& end) {
    if (m_tempRect) {
        m_scene->removeItem(m_tempRect);
        delete m_tempRect;
        m_tempRect = nullptr;
    }

    if (m_image.isNull()) return;

    // 计算像素坐标的矩形
    QRectF rect = QRectF(m_drawStart, end).normalized();

    // 最小尺寸检查
    if (rect.width() < 5 || rect.height() < 5) {
        return;
    }

    // 转换为归一化坐标
    double imgW = m_image.width();
    double imgH = m_image.height();
    BoundingBox bbox(
        (rect.x() + rect.width() / 2) / imgW,
        (rect.y() + rect.height() / 2) / imgH,
        rect.width() / imgW,
        rect.height() / imgH
    );
    bbox.clamp();

    Annotation ann(m_currentClass, bbox);

    // 如果是Pose模式，初始化关键点
    if (m_skeleton.keypointCount() > 0) {
        ann.setKeypointCount(m_skeleton.keypointCount());
    }

    addAnnotation(ann);
    int newIndex = m_annotations.size() - 1;
    setSelectedAnnotation(newIndex);

    // 请求选择类别，传递当前鼠标位置
    emit classSelectRequested(newIndex, QCursor::pos());
}

int CanvasView::findAnnotationAt(const QPointF& scenePos) {
    if (m_image.isNull()) return -1;

    double imgW = m_image.width();
    double imgH = m_image.height();

    // 转换为归一化坐标
    double nx = scenePos.x() / imgW;
    double ny = scenePos.y() / imgH;

    for (int i = m_annotations.size() - 1; i >= 0; --i) {
        const BoundingBox& bbox = m_annotations[i].boundingBox();
        if (nx >= bbox.left() && nx <= bbox.right() &&
            ny >= bbox.top() && ny <= bbox.bottom()) {
            return i;
        }
    }
    return -1;
}

int CanvasView::findPointAt(int annIndex, const QPointF& scenePos) {
    if (annIndex < 0 || annIndex >= m_annotations.size()) return -1;
    if (m_image.isNull()) return -1;

    const Annotation& ann = m_annotations[annIndex];
    double imgW = m_image.width();
    double imgH = m_image.height();

    // 在场景像素坐标中计算 (固定屏幕大小)
    double scale = transform().m11();
    double threshold = 8.0 / scale;  // 屏幕上固定8像素，与角点视觉一致

    // 检查边界框角点（转换为场景像素坐标）
    const BoundingBox& bbox = ann.boundingBox();
    QPointF corners[4] = {
        {bbox.left() * imgW, bbox.top() * imgH},
        {bbox.right() * imgW, bbox.top() * imgH},
        {bbox.right() * imgW, bbox.bottom() * imgH},
        {bbox.left() * imgW, bbox.bottom() * imgH}
    };

    for (int i = 0; i < 4; ++i) {
        double dist = std::sqrt(std::pow(scenePos.x() - corners[i].x(), 2) +
                                std::pow(scenePos.y() - corners[i].y(), 2));
        if (dist < threshold) {
            return i;
        }
    }

    // 检查关键点
    for (int i = 0; i < ann.keypointCount(); ++i) {
        const Keypoint& kp = ann.keypoints()[i];
        if (kp.isValid()) {
            double kpx = kp.x() * imgW;
            double kpy = kp.y() * imgH;
            double dist = std::sqrt(std::pow(scenePos.x() - kpx, 2) +
                                    std::pow(scenePos.y() - kpy, 2));
            if (dist < threshold) {
                return i + 4;
            }
        }
    }

    return -1;  // 整体
}

void CanvasView::applyZoom(double factor, const QPointF& center) {
    scale(factor, factor);
    emit zoomChanged(transform().m11());
    updateAnnotationItems();
}

void CanvasView::updateCrossHair(const QPointF& scenePos) {
    if (m_image.isNull()) return;

    double imgW = m_image.width();
    double imgH = m_image.height();

    // 水平线：从左边缘到右边缘
    m_crossHairH->setLine(0, scenePos.y(), imgW, scenePos.y());
    // 垂直线：从上边缘到下边缘
    m_crossHairV->setLine(scenePos.x(), 0, scenePos.x(), imgH);
}

void CanvasView::leaveEvent(QEvent* event) {
    // 鼠标离开时隐藏十字线
    m_crossHairH->setVisible(false);
    m_crossHairV->setVisible(false);
    QGraphicsView::leaveEvent(event);
}

void CanvasView::enterEvent(QEnterEvent* event) {
    // 鼠标进入时，如果是边界框模式且启用十字线则显示
    if (m_mode == EditMode::DrawBBox && m_crossHairEnabled) {
        m_crossHairH->setVisible(true);
        m_crossHairV->setVisible(true);
    }
    QGraphicsView::enterEvent(event);
}

void CanvasView::setCrossHairEnabled(bool enabled) {
    m_crossHairEnabled = enabled;
    bool show = enabled && (m_mode == EditMode::DrawBBox);
    m_crossHairH->setVisible(show);
    m_crossHairV->setVisible(show);
}

void CanvasView::setCrossHairColor(const QColor& color) {
    m_crossHairColor = color;
    QPen pen(color, 1, Qt::DashLine);
    m_crossHairH->setPen(pen);
    m_crossHairV->setPen(pen);
}

void CanvasView::saveToHistory() {
    // 删除当前位置之后的历史
    while (m_history.size() > m_historyIndex + 1) {
        m_history.removeLast();
    }
    // 添加当前状态
    m_history.append(m_annotations);
    m_historyIndex = m_history.size() - 1;
    // 限制历史大小
    while (m_history.size() > MAX_HISTORY) {
        m_history.removeFirst();
        m_historyIndex--;
    }
}

void CanvasView::undo() {
    if (m_historyIndex > 0) {
        m_historyIndex--;
        m_annotations = m_history[m_historyIndex];
        m_selectedIndex = -1;
        updateAnnotationItems();
        emit annotationsChanged();
    }
}

void CanvasView::redo() {
    if (m_historyIndex < m_history.size() - 1) {
        m_historyIndex++;
        m_annotations = m_history[m_historyIndex];
        m_selectedIndex = -1;
        updateAnnotationItems();
        emit annotationsChanged();
    }
}
