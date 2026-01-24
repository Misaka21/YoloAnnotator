#include "Keypoint.h"

Keypoint::Keypoint() = default;

Keypoint::Keypoint(double x, double y, int visibility)
    : m_x(x), m_y(y), m_visibility(qBound(0, visibility, 2)), m_valid(true) {}
