#pragma once
#include <QPointF>

/**
 * @brief 关键点类，用于Pose标注
 */
class Keypoint {
public:
    Keypoint();
    Keypoint(double x, double y, int visibility = 2);

    // 坐标 (归一化0-1)
    double x() const { return m_x; }
    double y() const { return m_y; }
    void setX(double x) { m_x = x; }
    void setY(double y) { m_y = y; }
    void setPosition(double x, double y) { m_x = x; m_y = y; }
    QPointF position() const { return QPointF(m_x, m_y); }

    // 可见性: 0=不可见, 1=被遮挡, 2=完全可见
    int visibility() const { return m_visibility; }
    void setVisibility(int v) { m_visibility = qBound(0, v, 2); }

    // 循环切换可见性
    void cycleVisibility() { m_visibility = (m_visibility + 1) % 3; }

    // 是否为有效点(非占位符)
    bool isValid() const { return m_valid; }
    void setValid(bool valid) { m_valid = valid; }

private:
    double m_x = 0.0;
    double m_y = 0.0;
    int m_visibility = 2;
    bool m_valid = false;
};
