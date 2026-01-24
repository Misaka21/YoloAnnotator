#include "BoundingBox.h"
#include <algorithm>

BoundingBox::BoundingBox() = default;

BoundingBox::BoundingBox(double cx, double cy, double w, double h)
    : m_cx(cx), m_cy(cy), m_w(w), m_h(h) {}

void BoundingBox::setRect(double cx, double cy, double w, double h) {
    m_cx = cx;
    m_cy = cy;
    m_w = w;
    m_h = h;
}

QRectF BoundingBox::toRect() const {
    return QRectF(left(), top(), m_w, m_h);
}

BoundingBox BoundingBox::fromRect(const QRectF& rect) {
    return BoundingBox(
        rect.x() + rect.width() / 2.0,
        rect.y() + rect.height() / 2.0,
        rect.width(),
        rect.height()
    );
}

QRectF BoundingBox::toPixelRect(int imageWidth, int imageHeight) const {
    return QRectF(
        left() * imageWidth,
        top() * imageHeight,
        m_w * imageWidth,
        m_h * imageHeight
    );
}

BoundingBox BoundingBox::fromPixelRect(const QRectF& pixelRect, int imageWidth, int imageHeight) {
    return BoundingBox(
        (pixelRect.x() + pixelRect.width() / 2.0) / imageWidth,
        (pixelRect.y() + pixelRect.height() / 2.0) / imageHeight,
        pixelRect.width() / imageWidth,
        pixelRect.height() / imageHeight
    );
}

void BoundingBox::clamp() {
    // 限制边界到[0,1]
    double l = std::max(0.0, left());
    double t = std::max(0.0, top());
    double r = std::min(1.0, right());
    double b = std::min(1.0, bottom());

    m_w = r - l;
    m_h = b - t;
    m_cx = l + m_w / 2.0;
    m_cy = t + m_h / 2.0;
}

bool BoundingBox::isValid() const {
    return m_w > 0 && m_h > 0 &&
           m_cx >= 0 && m_cx <= 1 &&
           m_cy >= 0 && m_cy <= 1;
}
