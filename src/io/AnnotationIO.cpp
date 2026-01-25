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

    // 自动检测该行的关键点数量和维度
    int extraValues = parts.size() - 5;
    int lineKpCount = 0;
    int lineDim = m_kptDim;  // 使用设置的维度

    // 优先使用设置的维度
    if (m_kptDim == 2 && extraValues >= 2 && extraValues % 2 == 0) {
        lineKpCount = extraValues / 2;
        lineDim = 2;
    } else if (m_kptDim == 3 && extraValues >= 3 && extraValues % 3 == 0) {
        lineKpCount = extraValues / 3;
        lineDim = 3;
    } else {
        // 自动检测: 优先检测3值格式，然后2值格式
        if (extraValues >= 3 && extraValues % 3 == 0) {
            lineKpCount = extraValues / 3;
            lineDim = 3;
        } else if (extraValues >= 2 && extraValues % 2 == 0) {
            lineKpCount = extraValues / 2;
            lineDim = 2;
        }
    }

    // 解析关键点（按该行实际数量）
    if (lineKpCount > 0) {
        ann.setKeypointCount(lineKpCount);
        int kpStartIdx = 5;

        for (int i = 0; i < lineKpCount; ++i) {
            int idx = kpStartIdx + i * lineDim;
            double kpX = parts[idx].toDouble();
            double kpY = parts[idx + 1].toDouble();
            int kpVis = (lineDim >= 3) ? parts[idx + 2].toInt() : 2;  // 无visibility时默认为2(可见)

            Keypoint kp(kpX, kpY, kpVis);

            // 只有坐标非零或可见性非零时才标记为有效
            // (0, 0, 0) 视为无效占位符
            bool isValidKp = (kpX != 0.0 || kpY != 0.0 || kpVis != 0);
            kp.setValid(isValidKp);

            ann.setKeypoint(i, kp);
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

    // 只输出有效的关键点
    if (ann.hasPose()) {
        for (const Keypoint& kp : ann.keypoints()) {
            if (kp.isValid()) {
                out << " " << kp.x()
                    << " " << kp.y();
                if (m_kptDim >= 3) {
                    out << " " << kp.visibility();
                }
            }
        }
    }

    return line;
}

AnnotationIO::Format AnnotationIO::detectFormat(const QString& txtPath, int& outKeypointCount, int& outKptDim) {
    QFile file(txtPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        outKeypointCount = 0;
        outKptDim = 3;
        return Detection;
    }

    // 扫描所有行，找最大关键点数和维度
    int maxKp = 0;
    int detectedDim = 3;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() > 5) {
            int extraValues = parts.size() - 5;
            // 优先检测3值格式
            if (extraValues % 3 == 0) {
                int kp = extraValues / 3;
                if (kp > maxKp) {
                    maxKp = kp;
                    detectedDim = 3;
                }
            }
            // 如果3值格式不匹配，尝试2值格式
            else if (extraValues % 2 == 0) {
                int kp = extraValues / 2;
                if (kp > maxKp) {
                    maxKp = kp;
                    detectedDim = 2;
                }
            }
        }
    }

    outKeypointCount = maxKp;
    outKptDim = detectedDim;
    return (maxKp > 0) ? Pose : Detection;
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
