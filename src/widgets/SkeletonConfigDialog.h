#pragma once
#include <QDialog>
#include <QComboBox>
#include <QTableWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QPushButton>
#include "core/SkeletonConfig.h"

class DraggablePoint;

/**
 * @brief 骨架配置对话框
 */
class SkeletonConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit SkeletonConfigDialog(QWidget* parent = nullptr);

    void setConfig(const SkeletonConfig& config);
    SkeletonConfig config() const { return m_config; }

    // 供 DraggablePoint 调用
    void onPointMoved(int index, const QPointF& newPos);

private slots:
    void onLoadPreset();
    void onSaveToYaml();
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
    QPushButton* m_saveYamlBtn;

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
    QVector<DraggablePoint*> m_previewPoints;
    QVector<QGraphicsLineItem*> m_previewLines;

    // 关键点位置（预览用）
    QVector<QPointF> m_previewPositions;

    SkeletonConfig m_config;

    void setupUI();
    void refreshKeypointTable();
    void refreshBoneTable();
    void updateBoneComboBoxes();
    void updatePreviewLines();

    // 获取默认预览位置
    QVector<QPointF> getDefaultPositions(int count) const;
};

/**
 * @brief 可拖动的关键点
 */
class DraggablePoint : public QGraphicsEllipseItem {
public:
    DraggablePoint(int index, SkeletonConfigDialog* dialog, qreal x, qreal y, qreal w, qreal h);

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    int m_index;
    SkeletonConfigDialog* m_dialog;
};
