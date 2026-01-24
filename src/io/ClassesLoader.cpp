#include "ClassesLoader.h"
#include <QFile>
#include <QTextStream>

ClassesLoader::ClassesLoader() = default;

bool ClassesLoader::load(const QString& filePath) {
    QFile file(filePath);
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

    return true;
}

QString ClassesLoader::className(int index) const {
    if (index >= 0 && index < m_classNames.size()) {
        return m_classNames[index];
    }
    return QString("class_%1").arg(index);
}

int ClassesLoader::indexOf(const QString& name) const {
    return m_classNames.indexOf(name);
}

QColor ClassesLoader::classColor(int index) const {
    return generateColor(index);
}

void ClassesLoader::addClass(const QString& name) {
    if (!m_classNames.contains(name)) {
        m_classNames.append(name);
    }
}

void ClassesLoader::setClassNames(const QStringList& names) {
    m_classNames = names;
}

void ClassesLoader::clear() {
    m_classNames.clear();
}

bool ClassesLoader::save(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    for (const QString& name : m_classNames) {
        out << name << "\n";
    }

    return true;
}

QColor ClassesLoader::generateColor(int index) {
    // 使用HSV色彩空间生成不同的颜色
    static const QList<QColor> predefinedColors = {
        QColor(255, 0, 0),      // 红
        QColor(0, 255, 0),      // 绿
        QColor(0, 0, 255),      // 蓝
        QColor(255, 255, 0),    // 黄
        QColor(255, 0, 255),    // 品红
        QColor(0, 255, 255),    // 青
        QColor(255, 128, 0),    // 橙
        QColor(128, 0, 255),    // 紫
        QColor(0, 255, 128),    // 春绿
        QColor(255, 0, 128),    // 玫红
    };

    if (index < predefinedColors.size()) {
        return predefinedColors[index];
    }

    // 超出预定义颜色后，使用HSV生成
    int hue = (index * 37) % 360;  // 黄金角度分布
    return QColor::fromHsv(hue, 200, 255);
}
