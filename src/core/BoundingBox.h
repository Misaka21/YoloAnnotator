#pragma once
#include <QRectF>

/**
 * @brief 边界框类，使用YOLO格式（中心点+宽高，归一化坐标）
 */
class BoundingBox {
public:
    BoundingBox();
    BoundingBox(double cx, double cy, double w, double h);

    // YOLO格式: 中心点+宽高 (归一化0-1)
    double centerX() const { return m_cx; }
    double centerY() const { return m_cy; }
    double width() const { return m_w; }
    double height() const { return m_h; }

    void setCenterX(double cx) { m_cx = cx; }
    void setCenterY(double cy) { m_cy = cy; }
    void setWidth(double w) { m_w = w; }
    void setHeight(double h) { m_h = h; }
    void setRect(double cx, double cy, double w, double h);

    // 边界坐标
    double left() const { return m_cx - m_w / 2.0; }
    double right() const { return m_cx + m_w / 2.0; }
    double top() const { return m_cy - m_h / 2.0; }
    double bottom() const { return m_cy + m_h / 2.0; }

    // 转换为QRectF (左上角+宽高，归一化)
    QRectF toRect() const;
    static BoundingBox fromRect(const QRectF& rect);

    // 转换为像素坐标的QRectF
    QRectF toPixelRect(int imageWidth, int imageHeight) const;
    static BoundingBox fromPixelRect(const QRectF& pixelRect, int imageWidth, int imageHeight);

    // 边界检查与裁剪到[0,1]
    void clamp();
    bool isValid() const;

private:
    double m_cx = 0.0;  // 中心X
    double m_cy = 0.0;  // 中心Y
    double m_w = 0.0;   // 宽度
    double m_h = 0.0;   // 高度
};
