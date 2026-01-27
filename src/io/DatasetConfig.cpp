#include "DatasetConfig.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <yaml-cpp/yaml.h>

DatasetConfig::DatasetConfig() = default;

bool DatasetConfig::loadYAML(const QString& path) {
    clear();

    QFile file(path);
    if (!file.exists()) {
        return false;
    }

    // 使用 QFile 读取以正确处理中文路径
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QByteArray content = file.readAll();
    file.close();

    try {
        YAML::Node root = YAML::Load(content.toStdString());

        m_projectPath = path;

        // task 类型
        if (root["task"]) {
            m_taskType = taskTypeFromString(QString::fromStdString(root["task"].as<std::string>()));
        }

        // path (数据集根目录)
        if (root["path"]) {
            m_datasetPath = QString::fromStdString(root["path"].as<std::string>());
        } else {
            // 如果没有 path，使用 YAML 文件所在目录
            m_datasetPath = QFileInfo(path).absolutePath();
        }

        // train/val/test 路径
        if (root["train"]) {
            m_trainPath = QString::fromStdString(root["train"].as<std::string>());
        }
        if (root["val"]) {
            m_valPath = QString::fromStdString(root["val"].as<std::string>());
        }
        if (root["test"]) {
            m_testPath = QString::fromStdString(root["test"].as<std::string>());
        }

        // names (类别名称)
        if (root["names"]) {
            const YAML::Node& names = root["names"];
            if (names.IsMap()) {
                // 格式: names: {0: person, 1: car}
                int maxIdx = -1;
                for (auto it = names.begin(); it != names.end(); ++it) {
                    int idx = it->first.as<int>();
                    if (idx > maxIdx) maxIdx = idx;
                }
                m_classNames.resize(maxIdx + 1);
                for (auto it = names.begin(); it != names.end(); ++it) {
                    int idx = it->first.as<int>();
                    m_classNames[idx] = QString::fromStdString(it->second.as<std::string>());
                }
            } else if (names.IsSequence()) {
                // 格式: names: [person, car]
                for (size_t i = 0; i < names.size(); ++i) {
                    m_classNames.append(QString::fromStdString(names[i].as<std::string>()));
                }
            }
        }

        // nc (类别数量，可选)
        // 如果 names 已经解析了，不需要 nc

        // kpt_shape (Pose)
        if (root["kpt_shape"]) {
            const YAML::Node& shape = root["kpt_shape"];
            if (shape.IsSequence() && shape.size() >= 2) {
                m_kptShape[0] = shape[0].as<int>();
                m_kptShape[1] = shape[1].as<int>();
                m_taskType = Pose;
            }
        }

        // keypoints (关键点名称)
        QStringList keypointNames;
        if (root["keypoints"]) {
            const YAML::Node& kps = root["keypoints"];
            if (kps.IsSequence()) {
                for (size_t i = 0; i < kps.size(); ++i) {
                    keypointNames.append(QString::fromStdString(kps[i].as<std::string>()));
                }
            }
        }

        // skeleton (骨架连接)
        QVector<QPair<int,int>> bones;
        if (root["skeleton"]) {
            const YAML::Node& sk = root["skeleton"];
            if (sk.IsSequence()) {
                for (size_t i = 0; i < sk.size(); ++i) {
                    const YAML::Node& bone = sk[i];
                    if (bone.IsSequence() && bone.size() >= 2) {
                        // YAML 中骨架索引是 1-based
                        int from = bone[0].as<int>() - 1;
                        int to = bone[1].as<int>() - 1;
                        bones.append(qMakePair(from, to));
                    }
                }
            }
        }

        // skeleton_colors (可选)
        QVector<QColor> boneColors;
        if (root["skeleton_colors"]) {
            const YAML::Node& colors = root["skeleton_colors"];
            if (colors.IsSequence()) {
                for (size_t i = 0; i < colors.size(); ++i) {
                    const YAML::Node& c = colors[i];
                    if (c.IsSequence() && c.size() >= 3) {
                        int r = c[0].as<int>();
                        int g = c[1].as<int>();
                        int b = c[2].as<int>();
                        boneColors.append(QColor(r, g, b));
                    }
                }
            }
        }

        // 构建 SkeletonConfig
        if (!keypointNames.isEmpty() || m_kptShape[0] > 0) {
            parseSkeletonFromYAML(keypointNames, bones, boneColors);
        }

        m_modified = false;
        return true;

    } catch (const YAML::Exception& e) {
        qWarning("YAML parse error: %s", e.what());
        return false;
    }
}

bool DatasetConfig::loadClassesTxt(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    m_classNames.clear();
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            m_classNames.append(line);
        }
    }

    // 设置默认路径（classes.txt 所在目录）
    QFileInfo fi(path);
    m_datasetPath = fi.absolutePath();
    m_projectPath.clear();  // 没有项目文件

    m_taskType = Detection;
    m_modified = false;

    return true;
}

bool DatasetConfig::saveYAML(const QString& path) const {
    YAML::Emitter out;
    out << YAML::BeginMap;

    // task
    out << YAML::Key << "task" << YAML::Value << taskTypeString().toStdString();

    // path
    if (!m_datasetPath.isEmpty()) {
        out << YAML::Key << "path" << YAML::Value << m_datasetPath.toStdString();
    }

    // train/val/test
    if (!m_trainPath.isEmpty()) {
        out << YAML::Key << "train" << YAML::Value << m_trainPath.toStdString();
    }
    if (!m_valPath.isEmpty()) {
        out << YAML::Key << "val" << YAML::Value << m_valPath.toStdString();
    }
    if (!m_testPath.isEmpty()) {
        out << YAML::Key << "test" << YAML::Value << m_testPath.toStdString();
    }

    // names (使用 map 格式)
    out << YAML::Key << "names" << YAML::Value << YAML::BeginMap;
    for (int i = 0; i < m_classNames.size(); ++i) {
        out << YAML::Key << i << YAML::Value << m_classNames[i].toStdString();
    }
    out << YAML::EndMap;

    // Pose 特有字段
    if (m_taskType == Pose && m_kptShape[0] > 0) {
        // kpt_shape
        out << YAML::Key << "kpt_shape" << YAML::Value << YAML::Flow
            << YAML::BeginSeq << m_kptShape[0] << m_kptShape[1] << YAML::EndSeq;

        // keypoints
        if (m_skeleton.keypointCount() > 0) {
            out << YAML::Key << "keypoints" << YAML::Value << YAML::BeginSeq;
            for (int i = 0; i < m_skeleton.keypointCount(); ++i) {
                out << m_skeleton.keypointName(i).toStdString();
            }
            out << YAML::EndSeq;

            // skeleton (1-based index)
            if (!m_skeleton.bones().isEmpty()) {
                out << YAML::Key << "skeleton" << YAML::Value << YAML::BeginSeq;
                for (const auto& bone : m_skeleton.bones()) {
                    out << YAML::Flow << YAML::BeginSeq
                        << (bone.from + 1) << (bone.to + 1)
                        << YAML::EndSeq;
                }
                out << YAML::EndSeq;

                // skeleton_colors
                out << YAML::Key << "skeleton_colors" << YAML::Value << YAML::BeginSeq;
                for (const auto& bone : m_skeleton.bones()) {
                    out << YAML::Flow << YAML::BeginSeq
                        << bone.color.red() << bone.color.green() << bone.color.blue()
                        << YAML::EndSeq;
                }
                out << YAML::EndSeq;
            }
        }
    }

    out << YAML::EndMap;

    // 写入文件
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream << QString::fromStdString(out.c_str());

    return true;
}

QString DatasetConfig::resolvedTrainPath() const {
    if (m_trainPath.isEmpty()) return QString();

    // 如果 datasetPath 是相对路径，则相对于 YAML 文件所在目录
    QString basePath = m_datasetPath;
    if (!m_projectPath.isEmpty() && !QDir::isAbsolutePath(m_datasetPath)) {
        QDir yamlDir = QFileInfo(m_projectPath).absoluteDir();
        basePath = yamlDir.absoluteFilePath(m_datasetPath);
    }

    QDir base(basePath);
    return base.absoluteFilePath(m_trainPath);
}

QString DatasetConfig::resolvedValPath() const {
    if (m_valPath.isEmpty()) return QString();

    // 如果 datasetPath 是相对路径，则相对于 YAML 文件所在目录
    QString basePath = m_datasetPath;
    if (!m_projectPath.isEmpty() && !QDir::isAbsolutePath(m_datasetPath)) {
        QDir yamlDir = QFileInfo(m_projectPath).absoluteDir();
        basePath = yamlDir.absoluteFilePath(m_datasetPath);
    }

    QDir base(basePath);
    return base.absoluteFilePath(m_valPath);
}

QString DatasetConfig::resolvedTestPath() const {
    if (m_testPath.isEmpty()) return QString();

    // 如果 datasetPath 是相对路径，则相对于 YAML 文件所在目录
    QString basePath = m_datasetPath;
    if (!m_projectPath.isEmpty() && !QDir::isAbsolutePath(m_datasetPath)) {
        QDir yamlDir = QFileInfo(m_projectPath).absoluteDir();
        basePath = yamlDir.absoluteFilePath(m_datasetPath);
    }

    QDir base(basePath);
    return base.absoluteFilePath(m_testPath);
}

QString DatasetConfig::className(int index) const {
    if (index >= 0 && index < m_classNames.size()) {
        return m_classNames[index];
    }
    return QString("class_%1").arg(index);
}

int DatasetConfig::indexOf(const QString& name) const {
    return m_classNames.indexOf(name);
}

QColor DatasetConfig::classColor(int index) const {
    if (m_classColors.contains(index)) {
        return m_classColors[index];
    }
    if (index >= 0 && index < m_classNames.size()) {
        return colorFromPrefix(m_classNames[index]);
    }
    return Qt::green;
}

void DatasetConfig::setClassColor(int index, const QColor& color) {
    m_classColors[index] = color;
    m_modified = true;
}

QString DatasetConfig::taskTypeString() const {
    switch (m_taskType) {
        case Detection: return "detect";
        case Segmentation: return "segment";
        case Pose: return "pose";
    }
    return "detect";
}

DatasetConfig::TaskType DatasetConfig::taskTypeFromString(const QString& str) {
    QString lower = str.toLower();
    if (lower == "pose" || lower == "keypoint" || lower == "keypoints") {
        return Pose;
    } else if (lower == "segment" || lower == "segmentation") {
        return Segmentation;
    }
    return Detection;
}

void DatasetConfig::setKptShape(int count, int dim) {
    m_kptShape[0] = count;
    m_kptShape[1] = dim;
    m_modified = true;
}

void DatasetConfig::clear() {
    m_taskType = Detection;
    m_projectPath.clear();
    m_datasetPath.clear();
    m_trainPath.clear();
    m_valPath.clear();
    m_testPath.clear();
    m_classNames.clear();
    m_classColors.clear();
    m_kptShape[0] = 0;
    m_kptShape[1] = 3;
    m_skeleton = SkeletonConfig();
    m_modified = false;
}

DatasetConfig DatasetConfig::createDetection(const QStringList& classNames) {
    DatasetConfig config;
    config.m_taskType = Detection;
    config.m_classNames = classNames;
    return config;
}

DatasetConfig DatasetConfig::createPose(const QStringList& classNames, int keypointCount) {
    DatasetConfig config;
    config.m_taskType = Pose;
    config.m_classNames = classNames;
    config.m_kptShape[0] = keypointCount;
    config.m_kptShape[1] = 3;
    config.m_skeleton = SkeletonConfig::createCustom(keypointCount);
    return config;
}

QColor DatasetConfig::colorFromPrefix(const QString& className) {
    QString lowerName = className.toLower();
    if (lowerName.startsWith("r-") || lowerName.startsWith("r_")) {
        return Qt::red;
    } else if (lowerName.startsWith("b-") || lowerName.startsWith("b_")) {
        return Qt::blue;
    } else if (lowerName.startsWith("w-") || lowerName.startsWith("w_") || lowerName.startsWith("w")) {
        return Qt::white;
    } else if (lowerName.startsWith("p-") || lowerName.startsWith("p_") || lowerName.startsWith("p")) {
        return QColor(255, 105, 180);  // 粉色
    }
    return Qt::green;
}

void DatasetConfig::parseSkeletonFromYAML(const QStringList& keypointNames,
                                          const QVector<QPair<int,int>>& bones,
                                          const QVector<QColor>& colors) {
    int count = keypointNames.isEmpty() ? m_kptShape[0] : keypointNames.size();
    if (count <= 0) return;

    // 更新 kpt_shape
    m_kptShape[0] = count;

    // 设置关键点数量
    m_skeleton.setKeypointCount(count);

    // 设置关键点名称
    for (int i = 0; i < count && i < keypointNames.size(); ++i) {
        KeypointDef def(keypointNames[i], Qt::red, true);
        m_skeleton.setKeypointDef(i, def);
    }

    // 设置骨架连接
    m_skeleton.clearBones();
    for (int i = 0; i < bones.size(); ++i) {
        QColor boneColor = (i < colors.size()) ? colors[i] : Qt::cyan;
        m_skeleton.addBone(bones[i].first, bones[i].second, boneColor);
    }
}
