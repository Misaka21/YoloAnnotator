#include "AnnotationIO.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>

AnnotationIO::AnnotationIO() = default;

QVector<Annotation> AnnotationIO::load(const QString& txtPath) {
    QVector<Annotation> annotations;
    QFile file(txtPath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return annotations;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            Annotation ann = parseLine(line);
            if (ann.boundingBox().isValid()) {
                annotations.append(ann);
            }
        }
    }

    return annotations;
}

Annotation AnnotationIO::parseLine(const QString& line) {
    QStringList parts = line.split(' ', Qt::SkipEmptyParts);
    Annotation ann;

    if (parts.size() < 5) return ann;  // 无效行

    // 解析类别和边界框
    ann.setClassId(parts[0].toInt());
    BoundingBox bbox(
        parts[1].toDouble(),  // cx
        parts[2].toDouble(),  // cy
        parts[3].toDouble(),  // w
        parts[4].toDouble()   // h
    );
    ann.setBoundingBox(bbox);

    // Pose格式: 解析关键点
    if (m_format == Pose && m_keypointCount > 0) {
        ann.setKeypointCount(m_keypointCount);
        int kpStartIdx = 5;

        for (int i = 0; i < m_keypointCount; ++i) {
            int idx = kpStartIdx + i * 3;
            if (idx + 2 < parts.size()) {
                Keypoint kp(
                    parts[idx].toDouble(),      // x
                    parts[idx + 1].toDouble(),  // y
                    parts[idx + 2].toInt()      // visibility
                );
                kp.setValid(true);
                ann.setKeypoint(i, kp);
            }
        }
    }

    return ann;
}

bool AnnotationIO::save(const QString& txtPath, const QVector<Annotation>& annotations) {
    // 如果没有标注，删除文件
    if (annotations.isEmpty()) {
        QFile::remove(txtPath);
        return true;
    }

    QFile file(txtPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out.setRealNumberPrecision(6);

    for (const Annotation& ann : annotations) {
        out << formatLine(ann) << "\n";
    }

    return true;
}

QString AnnotationIO::formatLine(const Annotation& ann) {
    QString line;
    QTextStream out(&line);
    out.setRealNumberPrecision(6);

    const BoundingBox& bbox = ann.boundingBox();
    out << ann.classId() << " "
        << bbox.centerX() << " "
        << bbox.centerY() << " "
        << bbox.width() << " "
        << bbox.height();

    // Pose格式: 输出关键点
    if (m_format == Pose && ann.hasPose()) {
        for (const Keypoint& kp : ann.keypoints()) {
            out << " " << kp.x()
                << " " << kp.y()
                << " " << kp.visibility();
        }
    }

    return line;
}

AnnotationIO::Format AnnotationIO::detectFormat(const QString& txtPath, int& outKeypointCount) {
    QFile file(txtPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        outKeypointCount = 0;
        return Detection;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() > 5) {
            // 超过5个值，可能是Pose格式
            int extraValues = parts.size() - 5;
            if (extraValues % 3 == 0) {
                outKeypointCount = extraValues / 3;
                return Pose;
            }
        }
        break;  // 只检查第一行
    }

    outKeypointCount = 0;
    return Detection;
}

QString AnnotationIO::getAnnotationPath(const QString& imagePath) {
    QFileInfo info(imagePath);
    QString dir = info.absolutePath();
    QString baseName = info.completeBaseName();

    // 检查 images -> labels 目录结构
    // 支持: dataset/images/xxx.jpg -> dataset/labels/xxx.txt
    // 支持: dataset/train/images/xxx.jpg -> dataset/train/labels/xxx.txt
    QString labelsDir = dir;
    if (dir.contains("/images") || dir.contains("\\images")) {
        labelsDir.replace("/images", "/labels");
        labelsDir.replace("\\images", "\\labels");

        QString labelsPath = labelsDir + "/" + baseName + ".txt";

        // 如果labels目录存在，或者同目录下没有txt，使用labels目录
        if (QDir(labelsDir).exists() || !QFile::exists(dir + "/" + baseName + ".txt")) {
            // 确保labels目录存在
            QDir().mkpath(labelsDir);
            return labelsPath;
        }
    }

    // 默认：同目录下的 .txt
    return dir + "/" + baseName + ".txt";
}
