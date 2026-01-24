#pragma once
#include "core/Annotation.h"
#include <QString>
#include <QVector>

/**
 * @brief YOLO标注格式读写
 */
class AnnotationIO {
public:
    // 格式类型
    enum Format {
        Detection,          // 标准YOLO检测格式
        Pose                // YOLO Pose格式
    };

    AnnotationIO();

    // 设置关键点数量 (Pose模式)
    void setKeypointCount(int count) { m_keypointCount = count; }
    int keypointCount() const { return m_keypointCount; }

    // 设置格式
    void setFormat(Format format) { m_format = format; }
    Format format() const { return m_format; }

    // 读取标注文件
    QVector<Annotation> load(const QString& txtPath);

    // 保存标注文件
    bool save(const QString& txtPath, const QVector<Annotation>& annotations);

    // 自动检测格式（根据文件内容）
    static Format detectFormat(const QString& txtPath, int& outKeypointCount);

    // 获取对应的标注文件路径
    static QString getAnnotationPath(const QString& imagePath);

private:
    Format m_format = Detection;
    int m_keypointCount = 0;

    // 解析单行
    Annotation parseLine(const QString& line);
    // 生成单行
    QString formatLine(const Annotation& ann);
};
