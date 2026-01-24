#include "Annotation.h"

int Annotation::s_nextId = 1;

Annotation::Annotation() : m_id(s_nextId++) {}

Annotation::Annotation(int classId, const BoundingBox& bbox)
    : m_id(s_nextId++), m_classId(classId), m_bbox(bbox) {}

void Annotation::setKeypoint(int index, const Keypoint& kp) {
    if (index >= 0 && index < m_keypoints.size()) {
        m_keypoints[index] = kp;
    }
}

void Annotation::setKeypointCount(int count) {
    m_keypoints.resize(count);
    for (auto& kp : m_keypoints) {
        kp.setValid(false);
    }
}

AnnotationType Annotation::type() const {
    return m_keypoints.isEmpty() ? AnnotationType::Detection : AnnotationType::Pose;
}
