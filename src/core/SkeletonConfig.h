#pragma once
#include <QString>
#include <QVector>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief 关键点定义
 */
struct KeypointDef {
    QString name;           // 关键点名称
    QColor color;           // 显示颜色
    bool required = true;   // 是否必须标注

    KeypointDef() : color(Qt::red) {}
    KeypointDef(const QString& n, const QColor& c = Qt::red, bool req = true)
        : name(n), color(c), required(req) {}
};

/**
 * @brief 骨架连接
 */
struct BoneConnection {
    int from;               // 起始关键点索引
    int to;                 // 结束关键点索引
    QColor color;           // 骨架连线颜色

    BoneConnection() : from(0), to(0), color(Qt::cyan) {}
    BoneConnection(int f, int t, const QColor& c = Qt::cyan)
        : from(f), to(t), color(c) {}
};

/**
 * @brief 骨架配置类
 */
class SkeletonConfig {
public:
    SkeletonConfig();

    // 关键点定义
    int keypointCount() const { return m_keypointDefs.size(); }
    void setKeypointCount(int count);
    const QVector<KeypointDef>& keypointDefs() const { return m_keypointDefs; }
    void setKeypointDef(int index, const KeypointDef& def);
    QString keypointName(int index) const;
    QColor keypointColor(int index) const;

    // 骨架连接
    const QVector<BoneConnection>& bones() const { return m_bones; }
    void addBone(int from, int to, const QColor& color = Qt::cyan);
    void setBone(int index, int from, int to, const QColor& color = Qt::cyan);
    void removeBone(int index);
    void clearBones();

    // 序列化
    QJsonObject toJson() const;
    static SkeletonConfig fromJson(const QJsonObject& json);

    // 保存/加载文件
    bool saveToFile(const QString& path) const;
    static SkeletonConfig loadFromFile(const QString& path);

    // 预设配置
    static SkeletonConfig createCOCO17();  // COCO 17关键点
    static SkeletonConfig createCustom(int count);

private:
    QVector<KeypointDef> m_keypointDefs;
    QVector<BoneConnection> m_bones;
};
