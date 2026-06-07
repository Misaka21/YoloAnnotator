#include "CanvasView.h"
#include "AnnotationItem.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QScrollBar>
#include <QContextMenuEvent>
#include <QPinchGesture>
#include <QTextDocument>
#include <QFile>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
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
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);  // 确保放大镜层级正确

    // 创建十字辅助线
    QPen crossPen(m_crossHairColor, 1, Qt::DashLine);
    m_crossHairH = m_scene->addLine(0, 0, 0, 0, crossPen);
    m_crossHairV = m_scene->addLine(0, 0, 0, 0, crossPen);
    m_crossHairH->setZValue(1000);
    m_crossHairV->setZValue(1000);
    m_crossHairH->setVisible(false);
    m_crossHairV->setVisible(false);

    // 触控板捏合手势（在 CanvasView 自身而非 viewport，避免干扰鼠标事件）
    grabGesture(Qt::PinchGesture);
}

CanvasView::~CanvasView() = default;

bool CanvasView::event(QEvent* e) {
    if (e->type() == QEvent::Gesture) {
        auto* ge = static_cast<QGestureEvent*>(e);
        if (QPinchGesture* pinch = static_cast<QPinchGesture*>(ge->gesture(Qt::PinchGesture))) {
            qreal factor = pinch->scaleFactor();
            qreal newScale = transform().m11() * factor;
            if (factor != 1.0 && newScale >= 0.1 && newScale <= 50.0) {
                QPointF center = mapToScene(pinch->centerPoint().toPoint());
                scale(factor, factor);
                QPointF d = mapFromScene(center) - pinch->centerPoint();
                horizontalScrollBar()->setValue(horizontalScrollBar()->value() + int(d.x()));
                verticalScrollBar()->setValue(verticalScrollBar()->value() + int(d.y()));
            }
            if (pinch->state() == Qt::GestureFinished) {
                emit zoomChanged(transform().m11());
                updateAnnotationItems();
            }
            return true;
        }
    }
    return QGraphicsView::event(e);
}

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

    // 添加图片（应用增强效果，缓存增强图给放大镜用）
    m_enhancedImage = applyEnhancementToImage(m_image);
    m_pixmapItem = m_scene->addPixmap(QPixmap::fromImage(m_enhancedImage));

    // 设置场景大小为图片大小
    m_scene->setSceneRect(0, 0, image.width(), image.height());

    zoomFit();
    updateAnnotationItems();
}

void CanvasView::setImage(const QString& imagePath) {
    // 使用 QFile 读取以正确处理特殊字符（如中括号[]）
    QFile file(imagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open image file:" << imagePath;
        emit imageLoadFailed(imagePath);
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QImage img;
    if (img.loadFromData(data)) {
        m_imagePath = imagePath;
        setImage(img);
    } else {
        // Qt 解码失败，尝试使用 OpenCV 作为后备
        qDebug() << "Qt failed to decode, trying OpenCV:" << imagePath;
        std::vector<uchar> buffer(data.begin(), data.end());
        cv::Mat mat = cv::imdecode(buffer, cv::IMREAD_COLOR);
        if (!mat.empty()) {
            // BGR -> RGB
            cv::cvtColor(mat, mat, cv::COLOR_BGR2RGB);
            img = QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888).copy();
            m_imagePath = imagePath;
            setImage(img);
        } else {
            qWarning() << "Failed to decode image (Qt and OpenCV both failed):" << imagePath;
            emit imageLoadFailed(imagePath);
        }
    }
}

void CanvasView::clearImage() {
    resetOperationState();

    m_image = QImage();
    m_imagePath.clear();
    m_transform.setImageSize(QSize());

    m_annotations.clear();
    m_selectedIndex = -1;
    m_history.clear();
    m_historyIndex = -1;

    m_scene->clear();
    m_pixmapItem = nullptr;
    m_annotationItems.clear();
    m_scene->setSceneRect(QRectF());

    // Re-create crosshair items after scene clear.
    QPen crossPen(m_crossHairColor, 1, Qt::DashLine);
    m_crossHairH = m_scene->addLine(0, 0, 0, 0, crossPen);
    m_crossHairV = m_scene->addLine(0, 0, 0, 0, crossPen);
    m_crossHairH->setZValue(1000);
    m_crossHairV->setZValue(1000);
    m_crossHairH->setVisible(false);
    m_crossHairV->setVisible(false);

    resetTransform();
    emit zoomChanged(1.0);
    emit annotationsChanged();
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
    // 清理所有进行中的操作状态
    resetOperationState();

    m_mode = mode;
    resetCursorForMode();

    // 边界框 / 关键点模式下显示十字线
    bool showCross = (mode == EditMode::DrawBBox || mode == EditMode::DrawKeypoint) && m_crossHairEnabled;
    m_crossHairH->setVisible(showCross);
    m_crossHairV->setVisible(showCross);

    viewport()->update();  // 切换模式时清除/刷新放大镜
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
    // 触控板双指滑动 → 平移画布（捏合缩放由 PinchGesture 处理）
    if (event->phase() != Qt::NoScrollPhase) {
        QPointF d = event->pixelDelta();
        if (d.isNull()) {
            d = event->angleDelta();
        }
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        event->accept();
        return;
    }

    // 鼠标滚轮 → 缩放
    double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
    double currentScale = transform().m11();
    double newScale = currentScale * factor;
    if (newScale < 0.1 || newScale > 50.0) {
        event->accept();
        return;
    }

    QPointF targetScenePos = mapToScene(event->position().toPoint());
    scale(factor, factor);
    QPointF targetViewPos = mapFromScene(targetScenePos);
    QPointF delta = targetViewPos - event->position();
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
            QPen drawPen(Qt::green, 2);
            drawPen.setCosmetic(true);  // 固定屏幕宽度，不随缩放变化
            m_tempRect = m_scene->addRect(QRectF(scenePos, scenePos),
                                          drawPen, QBrush(QColor(0, 255, 0, 50)));
            m_tempRect->setZValue(100);
        } else if (m_mode == EditMode::Select) {
            // 优先处理当前选中标注：吸附最近的控制点
            if (m_selectedIndex >= 0 && m_selectedIndex < m_annotations.size()
                && !m_image.isNull()) {
                const Annotation& selAnn = m_annotations[m_selectedIndex];
                const BoundingBox& selBbox = selAnn.boundingBox();
                double imgW = m_image.width();
                double imgH = m_image.height();
                double scale = transform().m11();
                double nx = scenePos.x() / imgW;
                double ny = scenePos.y() / imgH;
                // 给边缘留容差，角点圆点可能略超出bbox
                double epsX = 10.0 / scale / imgW;
                double epsY = 10.0 / scale / imgH;

                if (nx >= selBbox.left() - epsX && nx <= selBbox.right() + epsX &&
                    ny >= selBbox.top() - epsY && ny <= selBbox.bottom() + epsY) {

                    int nearestPt = findPointAt(m_selectedIndex, scenePos);

                    // Shift+点击关键点：删除
                    if (nearestPt >= 4 && (event->modifiers() & Qt::ShiftModifier)) {
                        int kpIdx = nearestPt - 4;
                        saveToHistory();
                        m_annotations[m_selectedIndex].keypoints()[kpIdx].setValid(false);
                        updateAnnotationItems();
                        emit annotationsChanged();
                        return;
                    }

                    // Alt+点击关键点：循环可见性
                    if (nearestPt >= 4 && (event->modifiers() & Qt::AltModifier)) {
                        int kpIdx = nearestPt - 4;
                        saveToHistory();
                        m_annotations[m_selectedIndex].keypoints()[kpIdx].cycleVisibility();
                        updateAnnotationItems();
                        emit annotationsChanged();
                        return;
                    }

                    // Ctrl+点击：强制整体移动框体
                    if (event->modifiers() & Qt::ControlModifier) {
                        nearestPt = -1;
                    }

                    // 拖动最近点（无修饰键），或整体移动（Ctrl）
                    if (nearestPt >= 0 || (event->modifiers() & Qt::ControlModifier)) {
                        m_dragging = true;
                        m_dragAnnIndex = m_selectedIndex;
                        m_dragPointIndex = nearestPt;
                        m_dragStartPos = scenePos;
                        saveToHistory();
                        return;
                    }

                    // 点击在框内但远离任何控制点且无Ctrl：不触发，防止误触
                    return;
                }
            }

            // 检查其他标注
            int annIdx = findAnnotationAt(scenePos);
            if (annIdx >= 0) {
                setSelectedAnnotation(annIdx);
                int nearestPt = findPointAt(annIdx, scenePos);

                // Ctrl+点击：强制整体移动
                if (event->modifiers() & Qt::ControlModifier) {
                    nearestPt = -1;
                }

                // 只有吸附到控制点或按下Ctrl才启动拖动，防止误触
                if (nearestPt >= 0 || (event->modifiers() & Qt::ControlModifier)) {
                    m_dragging = true;
                    m_dragAnnIndex = annIdx;
                    m_dragPointIndex = nearestPt;
                    m_dragStartPos = scenePos;
                    saveToHistory();
                }
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
        m_rightClickPos = event->pos();  // 记录初始位置用于区分点击和拖动
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
    m_cursorScenePos = scenePos;
    m_cursorViewPos = event->pos();
    emit mouseMoved(scenePos);

    // 更新十字辅助线（DrawBBox + DrawKeypoint 模式）
    if (m_mode == EditMode::DrawBBox || m_mode == EditMode::DrawKeypoint) {
        updateCrossHair(scenePos);
    }

    // 放大镜：强制刷新视口确保 drawForeground 重绘
    if (m_mode == EditMode::DrawKeypoint
        || (m_mode == EditMode::Select && m_dragging && m_dragPointIndex >= 4)) {
        viewport()->update();
    }

    // 悬停光标反馈：鼠标靠近控制点时显示十字光标
    if (!m_drawing && !m_panning && !m_dragging
        && m_mode == EditMode::Select
        && m_selectedIndex >= 0 && !m_image.isNull()) {
        int hoverPt = findPointAt(m_selectedIndex, scenePos);
        if (hoverPt >= 0) {
            setCursor(Qt::CrossCursor);
        } else {
            // 在选中框内但不在控制点附近：提示可用Ctrl移动
            const Annotation& ann = m_annotations[m_selectedIndex];
            const BoundingBox& bbox = ann.boundingBox();
            double imgW = m_image.width();
            double imgH = m_image.height();
            double nx = scenePos.x() / imgW;
            double ny = scenePos.y() / imgH;
            if (nx >= bbox.left() && nx <= bbox.right() &&
                ny >= bbox.top() && ny <= bbox.bottom()) {
                setCursor(Qt::OpenHandCursor);
            } else {
                resetCursorForMode();
            }
        }
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
            viewport()->update();  // 消除放大镜残留
        }
    } else if (event->button() == Qt::RightButton) {
        m_panning = false;

        // 检查是否是单击（用于显示上下文菜单）
        QPoint delta = event->pos() - m_rightClickPos;
        if (delta.manhattanLength() <= 5) {
            // 是单击，显示上下文菜单
            QPointF scenePos = mapToScene(event->pos());
            int annIdx = findAnnotationAt(scenePos);
            if (annIdx >= 0) {
                setSelectedAnnotation(annIdx);
                emit contextMenuRequested(annIdx, mapToGlobal(event->pos()));
            }
        }

        resetCursorForMode();
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
    // 右键菜单在 mouseReleaseEvent 中处理，这里直接忽略
    // 这样可以避免 Linux 上 contextMenuEvent 中断鼠标事件流的问题
    event->accept();
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

        // 检查是否有任何有效的关键点
        bool hasValidKeypoints = false;
        for (int k = 0; k < ann.keypointCount(); ++k) {
            if (ann.keypoints()[k].isValid()) {
                hasValidKeypoints = true;
                break;
            }
        }

        // 只有当标注有有效关键点时才绘制关键点和骨架
        if (hasValidKeypoints) {
            double viewScale = transform().m11();

            // 绘制关键点 - 使用精确的十字标记（类似标定板角点）
            for (int k = 0; k < ann.keypointCount(); ++k) {
                const Keypoint& kp = ann.keypoints()[k];
                if (kp.isValid()) {
                    double kx = kp.x() * imgW;
                    double ky = kp.y() * imgH;
                    QColor kpColor = m_skeleton.keypointColor(k);

                    // 根据可见性设置透明度
                    int alpha = (kp.visibility() == 0) ? 80 :
                                (kp.visibility() == 1) ? 160 : 255;
                    kpColor.setAlpha(alpha);

                    // 十字标记尺寸（屏幕像素，不随缩放变化）
                    double crossSize = 12.0 / viewScale;  // 十字臂长度
                    double penWidth = 2.0 / viewScale;    // 线宽
                    double dotRadius = 3.0 / viewScale;   // 中心点半径

                    // 外层十字（深色轮廓，增加对比度）
                    QColor outlineColor = Qt::black;
                    outlineColor.setAlpha(alpha);
                    double outlineWidth = (penWidth + 1.0 / viewScale);

                    // 水平线（外轮廓）
                    auto* hLineOut = m_scene->addLine(
                        kx - crossSize, ky, kx + crossSize, ky,
                        QPen(outlineColor, outlineWidth));
                    hLineOut->setZValue(3);
                    m_annotationItems.append(hLineOut);

                    // 垂直线（外轮廓）
                    auto* vLineOut = m_scene->addLine(
                        kx, ky - crossSize, kx, ky + crossSize,
                        QPen(outlineColor, outlineWidth));
                    vLineOut->setZValue(3);
                    m_annotationItems.append(vLineOut);

                    // 水平线（内层彩色）
                    auto* hLine = m_scene->addLine(
                        kx - crossSize, ky, kx + crossSize, ky,
                        QPen(kpColor, penWidth));
                    hLine->setZValue(4);
                    m_annotationItems.append(hLine);

                    // 垂直线（内层彩色）
                    auto* vLine = m_scene->addLine(
                        kx, ky - crossSize, kx, ky + crossSize,
                        QPen(kpColor, penWidth));
                    vLine->setZValue(4);
                    m_annotationItems.append(vLine);

                    // 中心小圆点（精确定位点）
                    auto* centerDot = m_scene->addEllipse(
                        kx - dotRadius, ky - dotRadius,
                        dotRadius * 2, dotRadius * 2,
                        QPen(outlineColor, 1.0 / viewScale),
                        QBrush(kpColor));
                    centerDot->setZValue(5);

                    // 添加 tooltip
                    QString visText = (kp.visibility() == 0) ? "不可见" :
                                      (kp.visibility() == 1) ? "遮挡" : "可见";
                    QString kpName = m_skeleton.keypointName(k);
                    if (kpName.isEmpty()) {
                        kpName = QString("关键点 %1").arg(k);
                    }
                    QString tooltip = QString("%1 [%2]\nShift+点击删除\nCtrl+点击切换可见性").arg(kpName).arg(visText);
                    centerDot->setToolTip(tooltip);

                    m_annotationItems.append(centerDot);

                    // 关键点序号标签
                    if (m_showKeypointLabels) {
                        double labelDist = crossSize + 4.0 / viewScale;
                        QFont kpFont("Segoe UI", 7);
                        kpFont.setWeight(QFont::Bold);
                        QString num = QString::number(k);
                        QColor kpLabelColor = kpColor;
                        kpLabelColor.setAlpha(alpha);
                        QColor kpLabelShadow(0, 0, 0, std::min(alpha, 160));

                        // 阴影
                        auto* shadow = m_scene->addSimpleText(num, kpFont);
                        shadow->setBrush(kpLabelShadow);
                        shadow->setPos(kx + labelDist + 1.0 / viewScale,
                                       ky - labelDist + 1.0 / viewScale);
                        shadow->setZValue(6);
                        m_annotationItems.append(shadow);

                        // 主文字
                        auto* label = m_scene->addSimpleText(num, kpFont);
                        label->setBrush(kpLabelColor);
                        label->setPos(kx + labelDist, ky - labelDist);
                        label->setZValue(7);
                        m_annotationItems.append(label);
                    }
                }
            }

            // 绘制骨架连线
            double boneWidth = 2.0 / viewScale;  // 固定屏幕宽度
            for (const auto& bone : m_skeleton.bones()) {
                if (bone.from < ann.keypointCount() && bone.to < ann.keypointCount()) {
                    const Keypoint& kp1 = ann.keypoints()[bone.from];
                    const Keypoint& kp2 = ann.keypoints()[bone.to];
                    if (kp1.isValid() && kp2.isValid()) {
                        double x1 = kp1.x() * imgW;
                        double y1 = kp1.y() * imgH;
                        double x2 = kp2.x() * imgW;
                        double y2 = kp2.y() * imgH;
                        auto* lineItem = m_scene->addLine(x1, y1, x2, y2, QPen(bone.color, boneWidth));
                        lineItem->setZValue(2);
                        m_annotationItems.append(lineItem);
                    }
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
    double scale = transform().m11();

    // 归一化坐标，留出角点圆点的容差（~10屏幕像素）
    double eps = 10.0 / scale / imgW;
    double epsY = 10.0 / scale / imgH;
    double nx = scenePos.x() / imgW;
    double ny = scenePos.y() / imgH;

    for (int i = m_annotations.size() - 1; i >= 0; --i) {
        const BoundingBox& bbox = m_annotations[i].boundingBox();
        if (nx >= bbox.left() - eps && nx <= bbox.right() + eps &&
            ny >= bbox.top() - epsY && ny <= bbox.bottom() + epsY) {
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
    const BoundingBox& bbox = ann.boundingBox();

    QPointF corners[4] = {
        {bbox.left() * imgW, bbox.top() * imgH},
        {bbox.right() * imgW, bbox.top() * imgH},
        {bbox.right() * imgW, bbox.bottom() * imgH},
        {bbox.left() * imgW, bbox.bottom() * imgH}
    };

    // 对所有候选点计算实际距离，取最近者
    // 关键点距离加权 1.2x，使角点在相近距离时有优先权
    double scale = transform().m11();
    double maxDist = 30.0 / scale;  // 统一阈值 ~30 屏幕像素
    const double kpBias = 1.2;       // 关键点加权（越大越偏向角点）

    double bestDist = maxDist;
    int    bestIdx  = -1;

    // 角点（不加权）
    for (int i = 0; i < 4; ++i) {
        double d = std::hypot(scenePos.x() - corners[i].x(),
                              scenePos.y() - corners[i].y());
        if (d < bestDist) { bestDist = d; bestIdx = i; }
    }

    // 关键点（加权，需要比角点明显更近才会赢）
    for (int i = 0; i < ann.keypointCount(); ++i) {
        const Keypoint& kp = ann.keypoints()[i];
        if (!kp.isValid()) continue;
        double d = std::hypot(scenePos.x() - kp.x() * imgW,
                              scenePos.y() - kp.y() * imgH) * kpBias;
        if (d < bestDist) { bestDist = d; bestIdx = i + 4; }
    }

    return bestIdx;
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

    // 重置所有进行中的操作状态
    resetOperationState();

    QGraphicsView::leaveEvent(event);
}

void CanvasView::enterEvent(QEnterEvent* event) {
    // 鼠标进入时，边界框/关键点模式下显示十字线
    if ((m_mode == EditMode::DrawBBox || m_mode == EditMode::DrawKeypoint) && m_crossHairEnabled) {
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

void CanvasView::resetCursorForMode() {
    if (m_mode == EditMode::Pan) {
        setCursor(Qt::OpenHandCursor);
    } else if (m_mode == EditMode::DrawBBox || m_mode == EditMode::DrawKeypoint) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void CanvasView::resetOperationState() {
    // 清理绘制状态
    if (m_drawing) {
        m_drawing = false;
        if (m_tempRect) {
            m_scene->removeItem(m_tempRect);
            delete m_tempRect;
            m_tempRect = nullptr;
        }
    }

    // 清理拖动状态
    m_dragging = false;
    m_dragAnnIndex = -1;
    m_dragPointIndex = -1;

    // 清理平移状态
    m_panning = false;

    // 重置光标
    resetCursorForMode();
}

void CanvasView::focusOutEvent(QFocusEvent* event) {
    // 窗口失去焦点时重置所有操作状态
    resetOperationState();
    QGraphicsView::focusOutEvent(event);
}

// ==================== 图像增强（仅显示，不修改原图） ====================

QImage CanvasView::applyEnhancementToImage(const QImage& src) const {
    if (src.isNull()) return src;

    // 1. 统一转换为 Format_RGB888，消除不同图片格式带来的像素布局差异
    QImage rgbImg = src.convertToFormat(QImage::Format_RGB888);

    // 2. 包装为 cv::Mat（无拷贝），再 clone 为连续 Mat
    cv::Mat mat(rgbImg.height(), rgbImg.width(), CV_8UC3,
                rgbImg.bits(),
                static_cast<size_t>(rgbImg.bytesPerLine()));
    cv::Mat rgb = mat.clone();

    // 3. 转到 float [0,1] 域
    cv::Mat fimg;
    rgb.convertTo(fimg, CV_32FC3, 1.0 / 255.0);

    // 4. 转到 HSV，亮度和饱和度在 HSV 域操作
    //    亮度(V): 乘性调整，V=0(纯黑)保持不变
    //    饱和度(S): 乘性缩放
    bool needHsv = (m_brightness != 0) || (std::abs(m_saturation / 100.0 - 1.0) > 0.001);
    if (needHsv) {
        cv::Mat hsv;
        cv::cvtColor(fimg, hsv, cv::COLOR_RGB2HSV);
        std::vector<cv::Mat> ch;
        cv::split(hsv, ch);  // ch[0]=H[0,360), ch[1]=S[0,1], ch[2]=V[0,1]

        // 亮度: V *= (1 + brightness/100)，V=0 保持为 0
        if (m_brightness != 0) {
            float vFactor = 1.0f + m_brightness / 100.0f;
            if (vFactor < 0.0f) vFactor = 0.0f;
            cv::multiply(ch[2], vFactor, ch[2]);
            cv::min(ch[2], 1.0, ch[2]);
        }

        // 饱和度: S *= saturation
        double saturation = m_saturation / 100.0;
        if (std::abs(saturation - 1.0) > 0.001) {
            cv::multiply(ch[1], static_cast<float>(saturation), ch[1]);
        }

        cv::merge(ch, hsv);
        cv::cvtColor(hsv, fimg, cv::COLOR_HSV2RGB);
    }

    // 5. 对比度: 在 RGB 域，以 0.5 灰点为轴心
    double contrast = m_contrast / 100.0;
    if (std::abs(contrast - 1.0) > 0.001) {
        fimg = (fimg - 0.5f) * static_cast<float>(contrast) + 0.5f;
    }

    // 6. 钳位到 [0,1] 并转回 uchar
    cv::max(fimg, 0.0, fimg);
    cv::min(fimg, 1.0, fimg);
    cv::Mat out8u;
    fimg.convertTo(out8u, CV_8UC3, 255.0);

    // 7. 确保内存连续后构造 QImage
    if (!out8u.isContinuous()) out8u = out8u.clone();
    return QImage(out8u.data, out8u.cols, out8u.rows,
                  static_cast<int>(out8u.step),
                  QImage::Format_RGB888).copy();
}

void CanvasView::applyEnhancement() {
    if (m_image.isNull() || !m_pixmapItem) return;

    m_enhancedImage = applyEnhancementToImage(m_image);
    m_pixmapItem->setPixmap(QPixmap::fromImage(m_enhancedImage));
}

void CanvasView::setBrightness(int value) {
    if (m_brightness != value) {
        m_brightness = value;
        applyEnhancement();
    }
}

void CanvasView::setContrast(int value) {
    if (m_contrast != value) {
        m_contrast = value;
        applyEnhancement();
    }
}

void CanvasView::setSaturation(int value) {
    if (m_saturation != value) {
        m_saturation = value;
        applyEnhancement();
    }
}

void CanvasView::resetEnhancement() {
    if (m_brightness == 0 && m_contrast == 100 && m_saturation == 100) return;
    m_brightness = 0;
    m_contrast = 100;
    m_saturation = 100;
    applyEnhancement();
}

void CanvasView::drawForeground(QPainter* painter, const QRectF&) {
    // 参照 LabelRoboMaster：ADDING_MODE 始终显示，NORMAL_MODE 仅拖动关键点时显示
    bool showLoupe = (m_mode == EditMode::DrawKeypoint)
                  || (m_mode == EditMode::Select && m_dragging && m_dragPointIndex >= 4);
    if (!showLoupe || m_image.isNull()) return;

    // 放大镜使用增强后的图像（与画布显示一致）
    const QImage& srcImg = m_enhancedImage.isNull() ? m_image : m_enhancedImage;

    QPoint pos = m_cursorViewPos;
    double scale = transform().m11();
    int srcSize = qMax(8, int(12.0 / scale));
    int cx = qBound(srcSize, int(m_cursorScenePos.x()), srcImg.width() - srcSize);
    int cy = qBound(srcSize, int(m_cursorScenePos.y()), srcImg.height() - srcSize);

    const int loupeSize = 120;
    QImage patch = srcImg.copy(cx - srcSize, cy - srcSize, srcSize * 2, srcSize * 2)
                       .scaled(loupeSize, loupeSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 放大镜位置（光标右下，贴边翻转到左上）
    int lx = pos.x() + 20, ly = pos.y() + 20;
    if (lx + patch.width()  > viewport()->width())  lx = pos.x() - 20 - patch.width();
    if (ly + patch.height() > viewport()->height()) ly = pos.y() - 20 - patch.height();

    painter->save();
    painter->resetTransform();

    // 阴影背景 + 放大图像 + 边框 + 中心十字
    painter->fillRect(lx - 1, ly - 1, patch.width() + 2, patch.height() + 2, QColor(0, 0, 0, 180));
    painter->drawImage(lx, ly, patch);
    painter->setPen(QPen(QColor(0, 255, 0, 220), 1));
    painter->drawRect(lx, ly, patch.width(), patch.height());
    int lcx = lx + patch.width() / 2, lcy = ly + patch.height() / 2;
    painter->drawLine(lcx, ly, lcx, ly + patch.height());
    painter->drawLine(lx, lcy, lx + patch.width(), lcy);

    painter->restore();
}
