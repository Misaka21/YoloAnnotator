#pragma once
#include <QString>
#include <QStringList>
#include <QColor>

/**
 * @brief 类别加载器，读取classes.txt文件
 */
class ClassesLoader {
public:
    ClassesLoader();

    // 加载classes.txt
    bool load(const QString& filePath);

    // 获取类别
    const QStringList& classNames() const { return m_classNames; }
    QString className(int index) const;
    int classCount() const { return m_classNames.size(); }

    // 查找类别索引
    int indexOf(const QString& name) const;

    // 获取类别颜色（自动分配）
    QColor classColor(int index) const;

    // 手动添加/设置类别
    void addClass(const QString& name);
    void setClassNames(const QStringList& names);
    void clear();

    // 保存到文件
    bool save(const QString& filePath) const;

private:
    QStringList m_classNames;

    static QColor generateColor(int index);
};
