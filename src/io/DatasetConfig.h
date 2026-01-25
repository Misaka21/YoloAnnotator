#pragma once
#include <QString>
#include <QStringList>
#include <QColor>
#include <QMap>
#include "core/SkeletonConfig.h"

/**
 * @brief 数据集配置类，支持 Ultralytics YOLO 标准 YAML 格式
 *
 * 支持的任务类型：
 * - detect: 目标检测
 * - segment: 实例分割
 * - pose: 姿态估计
 */
class DatasetConfig {
public:
    enum TaskType {
        Detection,      // 目标检测
        Segmentation,   // 实例分割
        Pose            // 姿态估计
    };

    DatasetConfig();

    // === 加载/保存 ===

    /**
     * @brief 从 YAML 文件加载配置
     * @param path YAML 文件路径
     * @return 是否成功
     */
    bool loadYAML(const QString& path);

    /**
     * @brief 从 classes.txt 加载类别
     * @param path classes.txt 文件路径
     * @return 是否成功
     */
    bool loadClassesTxt(const QString& path);

    /**
     * @brief 保存配置到 YAML 文件
     * @param path YAML 文件路径
     * @return 是否成功
     */
    bool saveYAML(const QString& path) const;

    // === 路径配置 ===

    QString datasetPath() const { return m_datasetPath; }
    void setDatasetPath(const QString& path) { m_datasetPath = path; }

    QString trainPath() const { return m_trainPath; }
    void setTrainPath(const QString& path) { m_trainPath = path; }

    QString valPath() const { return m_valPath; }
    void setValPath(const QString& path) { m_valPath = path; }

    QString testPath() const { return m_testPath; }
    void setTestPath(const QString& path) { m_testPath = path; }

    // 获取实际的图片目录路径
    QString resolvedTrainPath() const;
    QString resolvedValPath() const;
    QString resolvedTestPath() const;

    // === 类别访问 ===

    const QStringList& classNames() const { return m_classNames; }
    void setClassNames(const QStringList& names) { m_classNames = names; }
    QString className(int index) const;
    int classCount() const { return m_classNames.size(); }
    int indexOf(const QString& name) const;

    // 类别颜色（基于类名前缀自动分配）
    QColor classColor(int index) const;
    void setClassColor(int index, const QColor& color);

    // === 任务类型 ===

    TaskType taskType() const { return m_taskType; }
    void setTaskType(TaskType type) { m_taskType = type; }
    QString taskTypeString() const;
    static TaskType taskTypeFromString(const QString& str);

    bool isDetection() const { return m_taskType == Detection; }
    bool isSegmentation() const { return m_taskType == Segmentation; }
    bool isPose() const { return m_taskType == Pose; }

    // === Pose 配置 ===

    bool hasPose() const { return m_taskType == Pose && m_skeleton.keypointCount() > 0; }

    const SkeletonConfig& skeletonConfig() const { return m_skeleton; }
    SkeletonConfig& skeletonConfig() { return m_skeleton; }
    void setSkeletonConfig(const SkeletonConfig& config) { m_skeleton = config; }

    // kpt_shape: [数量, 维度]
    int keypointCount() const { return m_kptShape[0]; }
    int keypointDim() const { return m_kptShape[1]; }
    void setKptShape(int count, int dim = 3);

    // === 项目文件 ===

    QString projectPath() const { return m_projectPath; }
    bool isValid() const { return !m_classNames.isEmpty(); }
    bool isModified() const { return m_modified; }
    void setModified(bool modified = true) { m_modified = modified; }

    // === 工具方法 ===

    void clear();

    // 创建默认配置
    static DatasetConfig createDetection(const QStringList& classNames);
    static DatasetConfig createPose(const QStringList& classNames, int keypointCount);

private:
    // 任务类型
    TaskType m_taskType = Detection;

    // 路径配置
    QString m_projectPath;      // YAML 文件路径
    QString m_datasetPath;      // 数据集根目录 (path)
    QString m_trainPath;        // 训练集相对路径 (train)
    QString m_valPath;          // 验证集相对路径 (val)
    QString m_testPath;         // 测试集相对路径 (test)

    // 类别配置
    QStringList m_classNames;   // names
    QMap<int, QColor> m_classColors;

    // Pose 配置
    int m_kptShape[2] = {0, 3}; // kpt_shape: [count, dim]
    SkeletonConfig m_skeleton;

    // 状态
    bool m_modified = false;

    // 辅助方法
    static QColor colorFromPrefix(const QString& className);
    void parseSkeletonFromYAML(const QStringList& keypointNames,
                               const QVector<QPair<int,int>>& bones,
                               const QVector<QColor>& colors);
};
