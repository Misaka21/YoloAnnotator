#include "CoordinateTransform.h"

CoordinateTransform::CoordinateTransform() {
    updateTransforms();
}

void CoordinateTransform::setImageSize(const QSize& size) {
    m_imageSize = size;
    updateTransforms();
}

void CoordinateTransform::setImageSize(int width, int height) {
    setImageSize(QSize(width, height));
}

void CoordinateTransform::setViewTransform(const QTransform& transform) {
    m_img2view = transform;
    m_view2img = transform.inverted();
}

void CoordinateTransform::setScale(double scale) {
    m_scale = scale;
    updateTransforms();
}

void CoordinateTransform::setOffset(const QPointF& offset) {
    m_offset = offset;
    updateTransforms();
}

void CoordinateTransform::updateTransforms() {
    // 归一化 <-> 像素
    if (m_imageSize.isValid()) {
        m_norm2img = QTransform();
        m_norm2img.scale(m_imageSize.width(), m_imageSize.height());
        m_img2norm = m_norm2img.inverted();
    }

    // 像素 <-> 视图 (缩放 + 平移)
    m_img2view = QTransform();
    m_img2view.translate(m_offset.x(), m_offset.y());
    m_img2view.scale(m_scale, m_scale);
    m_view2img = m_img2view.inverted();
}

// 归一化 <-> 像素
QPointF CoordinateTransform::normalizedToImage(const QPointF& normalized) const {
    return m_norm2img.map(normalized);
}

QPointF CoordinateTransform::imageToNormalized(const QPointF& pixel) const {
    return m_img2norm.map(pixel);
}

QRectF CoordinateTransform::normalizedToImage(const QRectF& normalized) const {
    return m_norm2img.mapRect(normalized);
}

QRectF CoordinateTransform::imageToNormalized(const QRectF& pixel) const {
    return m_img2norm.mapRect(pixel);
}

// 像素 <-> 视图
QPointF CoordinateTransform::imageToView(const QPointF& pixel) const {
    return m_img2view.map(pixel);
}

QPointF CoordinateTransform::viewToImage(const QPointF& view) const {
    return m_view2img.map(view);
}

QRectF CoordinateTransform::imageToView(const QRectF& pixel) const {
    return m_img2view.mapRect(pixel);
}

QRectF CoordinateTransform::viewToImage(const QRectF& view) const {
    return m_view2img.mapRect(view);
}

// 归一化 <-> 视图 (组合)
QPointF CoordinateTransform::normalizedToView(const QPointF& normalized) const {
    return imageToView(normalizedToImage(normalized));
}

QPointF CoordinateTransform::viewToNormalized(const QPointF& view) const {
    return imageToNormalized(viewToImage(view));
}

QRectF CoordinateTransform::normalizedToView(const QRectF& normalized) const {
    return imageToView(normalizedToImage(normalized));
}

QRectF CoordinateTransform::viewToNormalized(const QRectF& view) const {
    return imageToNormalized(viewToImage(view));
}

void CoordinateTransform::resetView() {
    m_scale = 1.0;
    m_offset = QPointF(0, 0);
    updateTransforms();
}

void CoordinateTransform::fitToView(const QSize& viewSize) {
    if (!m_imageSize.isValid() || !viewSize.isValid()) {
        return;
    }

    // 计算适应缩放比例
    double scaleX = static_cast<double>(viewSize.width()) / m_imageSize.width();
    double scaleY = static_cast<double>(viewSize.height()) / m_imageSize.height();
    m_scale = qMin(scaleX, scaleY);

    // 居中
    double scaledWidth = m_imageSize.width() * m_scale;
    double scaledHeight = m_imageSize.height() * m_scale;
    m_offset.setX((viewSize.width() - scaledWidth) / 2.0);
    m_offset.setY((viewSize.height() - scaledHeight) / 2.0);

    updateTransforms();
}
