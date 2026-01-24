#pragma once
#include <QTransform>
#include <QPointF>
#include <QRectF>
#include <QSize>

/**
 * @brief 坐标变换系统
 * 处理三个坐标系之间的转换：
 * 1. 归一化坐标 [0,1] - YOLO标注格式使用
 * 2. 图像像素坐标 - 图片实际像素位置
 * 3. 视图坐标 - 屏幕显示位置（考虑缩放和平移）
 */
class CoordinateTransform {
public:
    CoordinateTransform();

    // 设置图像尺寸
    void setImageSize(const QSize& size);
    void setImageSize(int width, int height);
    QSize imageSize() const { return m_imageSize; }
    int imageWidth() const { return m_imageSize.width(); }
    int imageHeight() const { return m_imageSize.height(); }

    // 设置视图变换 (缩放、平移)
    void setViewTransform(const QTransform& transform);
    QTransform viewTransform() const { return m_img2view; }

    // 设置缩放和偏移
    void setScale(double scale);
    void setOffset(const QPointF& offset);
    double scale() const { return m_scale; }
    QPointF offset() const { return m_offset; }

    // 归一化[0,1] <-> 像素坐标
    QPointF normalizedToImage(const QPointF& normalized) const;
    QPointF imageToNormalized(const QPointF& pixel) const;
    QRectF normalizedToImage(const QRectF& normalized) const;
    QRectF imageToNormalized(const QRectF& pixel) const;

    // 像素坐标 <-> 视图坐标
    QPointF imageToView(const QPointF& pixel) const;
    QPointF viewToImage(const QPointF& view) const;
    QRectF imageToView(const QRectF& pixel) const;
    QRectF viewToImage(const QRectF& view) const;

    // 归一化 <-> 视图坐标 (组合变换)
    QPointF normalizedToView(const QPointF& normalized) const;
    QPointF viewToNormalized(const QPointF& view) const;
    QRectF normalizedToView(const QRectF& normalized) const;
    QRectF viewToNormalized(const QRectF& view) const;

    // 重置视图变换
    void resetView();

    // 适应视图大小
    void fitToView(const QSize& viewSize);

private:
    QSize m_imageSize;
    QTransform m_norm2img;   // 归一化 -> 像素
    QTransform m_img2norm;   // 像素 -> 归一化
    QTransform m_img2view;   // 像素 -> 视图
    QTransform m_view2img;   // 视图 -> 像素
    double m_scale = 1.0;
    QPointF m_offset;

    void updateTransforms();
};
