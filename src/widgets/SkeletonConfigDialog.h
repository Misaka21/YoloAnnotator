#pragma once
#include <QDialog>
#include <QComboBox>
#include <QTableWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPushButton>
#include "core/SkeletonConfig.h"

/**
 * @brief 骨架配置对话框
 */
class SkeletonConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit SkeletonConfigDialog(QWidget* parent = nullptr);

    void setConfig(const SkeletonConfig& config);
    SkeletonConfig config() const { return m_config; }

private slots:
    void onLoadPreset();
    void onAddKeypoint();
    void onRemoveKeypoint();
    void onAddBone();
    void onRemoveBone();
    void onKeypointCellChanged(int row, int column);
    void onBoneCellChanged(int row, int column);
    void updatePreview();

private:
    // 预设模板
    QComboBox* m_presetCombo;
    QPushButton* m_loadPresetBtn;

    // 关键点表格
    QTableWidget* m_keypointTable;
    QPushButton* m_addKeypointBtn;
    QPushButton* m_removeKeypointBtn;

    // 骨架连接表格
    QTableWidget* m_boneTable;
    QPushButton* m_addBoneBtn;
    QPushButton* m_removeBoneBtn;

    // 预览面板
    QGraphicsView* m_previewView;
    QGraphicsScene* m_previewScene;

    SkeletonConfig m_config;

    void setupUI();
    void refreshKeypointTable();
    void refreshBoneTable();
    void updateBoneComboBoxes();

    // 预览用的关键点位置（标准人形布局）
    QVector<QPointF> getPreviewPositions(int count) const;
};
