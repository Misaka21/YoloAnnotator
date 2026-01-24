#include "SkeletonConfig.h"
#include <QFile>
#include <QJsonDocument>

SkeletonConfig::SkeletonConfig() = default;

void SkeletonConfig::setKeypointCount(int count) {
    m_keypointDefs.resize(count);
    for (int i = 0; i < count; ++i) {
        if (m_keypointDefs[i].name.isEmpty()) {
            m_keypointDefs[i].name = QString("point_%1").arg(i);
            m_keypointDefs[i].color = Qt::red;
        }
    }
}

void SkeletonConfig::setKeypointDef(int index, const KeypointDef& def) {
    if (index >= 0 && index < m_keypointDefs.size()) {
        m_keypointDefs[index] = def;
    }
}

QString SkeletonConfig::keypointName(int index) const {
    if (index >= 0 && index < m_keypointDefs.size()) {
        return m_keypointDefs[index].name;
    }
    return QString();
}

QColor SkeletonConfig::keypointColor(int index) const {
    if (index >= 0 && index < m_keypointDefs.size()) {
        return m_keypointDefs[index].color;
    }
    return Qt::red;
}

void SkeletonConfig::addBone(int from, int to, const QColor& color) {
    if (from >= 0 && from < m_keypointDefs.size() &&
        to >= 0 && to < m_keypointDefs.size() && from != to) {
        m_bones.append(BoneConnection(from, to, color));
    }
}

void SkeletonConfig::removeBone(int index) {
    if (index >= 0 && index < m_bones.size()) {
        m_bones.removeAt(index);
    }
}

void SkeletonConfig::clearBones() {
    m_bones.clear();
}

QJsonObject SkeletonConfig::toJson() const {
    QJsonObject obj;

    QJsonArray keypointsArray;
    for (const auto& kp : m_keypointDefs) {
        QJsonObject kpObj;
        kpObj["name"] = kp.name;
        kpObj["color"] = kp.color.name();
        kpObj["required"] = kp.required;
        keypointsArray.append(kpObj);
    }
    obj["keypoints"] = keypointsArray;

    QJsonArray bonesArray;
    for (const auto& bone : m_bones) {
        QJsonArray boneArr;
        boneArr.append(bone.from);
        boneArr.append(bone.to);
        boneArr.append(bone.color.name());
        bonesArray.append(boneArr);
    }
    obj["bones"] = bonesArray;

    return obj;
}

SkeletonConfig SkeletonConfig::fromJson(const QJsonObject& json) {
    SkeletonConfig config;

    QJsonArray keypointsArray = json["keypoints"].toArray();
    for (const auto& kpVal : keypointsArray) {
        QJsonObject kpObj = kpVal.toObject();
        KeypointDef def;
        def.name = kpObj["name"].toString();
        def.color = QColor(kpObj["color"].toString());
        def.required = kpObj["required"].toBool(true);
        config.m_keypointDefs.append(def);
    }

    QJsonArray bonesArray = json["bones"].toArray();
    for (const auto& boneVal : bonesArray) {
        QJsonArray boneArr = boneVal.toArray();
        if (boneArr.size() >= 2) {
            BoneConnection bone;
            bone.from = boneArr[0].toInt();
            bone.to = boneArr[1].toInt();
            bone.color = boneArr.size() > 2 ? QColor(boneArr[2].toString()) : Qt::cyan;
            config.m_bones.append(bone);
        }
    }

    return config;
}

bool SkeletonConfig::saveToFile(const QString& path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    QJsonDocument doc(toJson());
    file.write(doc.toJson());
    return true;
}

SkeletonConfig SkeletonConfig::loadFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return SkeletonConfig();
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return fromJson(doc.object());
}

SkeletonConfig SkeletonConfig::createCOCO17() {
    SkeletonConfig config;

    // COCO 17个关键点
    QStringList names = {
        "nose", "left_eye", "right_eye", "left_ear", "right_ear",
        "left_shoulder", "right_shoulder", "left_elbow", "right_elbow",
        "left_wrist", "right_wrist", "left_hip", "right_hip",
        "left_knee", "right_knee", "left_ankle", "right_ankle"
    };

    for (const auto& name : names) {
        config.m_keypointDefs.append(KeypointDef(name, Qt::red));
    }

    // COCO骨架连接
    QVector<QPair<int, int>> connections = {
        {0, 1}, {0, 2}, {1, 3}, {2, 4},           // 头部
        {5, 6}, {5, 7}, {7, 9}, {6, 8}, {8, 10},  // 上半身
        {5, 11}, {6, 12}, {11, 12},               // 躯干
        {11, 13}, {13, 15}, {12, 14}, {14, 16}    // 下半身
    };

    for (const auto& conn : connections) {
        config.addBone(conn.first, conn.second, Qt::cyan);
    }

    return config;
}

SkeletonConfig SkeletonConfig::createCustom(int count) {
    SkeletonConfig config;
    config.setKeypointCount(count);
    return config;
}
