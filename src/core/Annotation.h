#pragma once
#include "BoundingBox.h"
#include "Keypoint.h"
#include <QVector>
#include <QString>

/**
 * @brief 标注类型
 */
enum class AnnotationType {
    Detection,  // 仅边界框
    Pose        // 边界框 + 关键点
};

/**
 * @brief 标注类，支持Detection和Pose两种格式
 */
class Annotation {
public:
    Annotation();
    Annotation(int classId, const BoundingBox& bbox);

    // 唯一标识
    int id() const { return m_id; }

    // 类别
    int classId() const { return m_classId; }
    void setClassId(int id) { m_classId = id; }

    // 边界框
    const BoundingBox& boundingBox() const { return m_bbox; }
    BoundingBox& boundingBox() { return m_bbox; }
    void setBoundingBox(const BoundingBox& bbox) { m_bbox = bbox; }

    // 关键点 (Pose模式)
    const QVector<Keypoint>& keypoints() const { return m_keypoints; }
    QVector<Keypoint>& keypoints() { return m_keypoints; }
    void setKeypoints(const QVector<Keypoint>& kps) { m_keypoints = kps; }
    void setKeypoint(int index, const Keypoint& kp);
    void setKeypointCount(int count);
    int keypointCount() const { return m_keypoints.size(); }

    // 类型判断
    AnnotationType type() const;
    bool hasPose() const { return !m_keypoints.isEmpty(); }

    // 跟踪ID (用于视频标注)
    int trackId() const { return m_trackId; }
    void setTrackId(int tid) { m_trackId = tid; }

    // 置信度 (自动标注时使用)
    float confidence() const { return m_confidence; }
    void setConfidence(float conf) { m_confidence = conf; }

private:
    int m_id;
    int m_classId = 0;
    BoundingBox m_bbox;
    QVector<Keypoint> m_keypoints;
    int m_trackId = -1;
    float m_confidence = -1.0f;

    static int s_nextId;
};
