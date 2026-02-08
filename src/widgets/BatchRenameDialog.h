#ifndef BATCHRENAMEDIALOG_H
#define BATCHRENAMEDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>

class BatchRenameDialog : public QDialog {
    Q_OBJECT

public:
    explicit BatchRenameDialog(QWidget* parent = nullptr);

    void setProjectPath(const QString& imagePath, const QString& labelsPath);
    void setImageFiles(const QStringList& imageFiles);

private slots:
    void onPatternChanged();
    void onPreview();
    void onRename();

private:
    void setupUI();
    void updatePreview();
    QString generateNewName(int index, const QString& extension);

private:
    // UI组件
    QLabel* m_projectInfoLabel;
    QLineEdit* m_prefixEdit;
    QSpinBox* m_startNumberSpinBox;
    QSpinBox* m_digitsSpinBox;
    QTableWidget* m_previewTable;
    QLabel* m_statusLabel;
    QPushButton* m_renameBtn;
    QPushButton* m_closeBtn;

    // 数据
    QString m_imagePath;
    QString m_labelsPath;
    QStringList m_imageFiles;  // 已按字典序排序
};

#endif // BATCHRENAMEDIALOG_H
