#ifndef SCREENCONFIGDIALOG_H
#define SCREENCONFIGDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QList>
#include <QListWidget>
#include <QTableWidget>
#include <QMouseEvent>
#include <QEvent>
#include <QModelIndex>
#include <QHeaderView>
#include <QApplication>
#include <QSpinBox>

#include "mainwindow.h"   // для DocGroup

namespace Ui {
    class ScreenConfigDialog;
}

class ScreenConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ScreenConfigDialog(const QList<DocGroup> &groups,
                                QWidget *parent = nullptr);
    ~ScreenConfigDialog();

    // ✅ Возвращает итоговый список групп.
    QList<DocGroup> groups() const { return m_groups; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onGroupSelectionChanged(int row);
    void onGroupOrderChanged();
    void onAddGroupClicked();
    void onRemoveGroupClicked();
    void onRenameGroupClicked();
    void onColumnsChanged(int value);


private:
    void rebuildGroupsList();
    void rebuildDocumentsTable();
    void refreshDocumentTable(int groupIndex);
    void updateButtonsState();

    // ✅ Внутренний DnD
    void setupDnD();
    bool handleTableDrag(QEvent *event);
    bool handleGroupListDrop(QEvent *event);

    // Данные
    QList<DocGroup> m_groups;
    int m_currentGroupIndex = -1;

    // UI
    QListWidget     *m_groupsList        = nullptr;
    QTableWidget    *m_documentsTable    = nullptr;
    QSpinBox        *m_columnsSpin       = nullptr;
    QPushButton     *m_addGroupBtn       = nullptr;
    QPushButton     *m_removeGroupBtn    = nullptr;
    QPushButton     *m_renameGroupBtn    = nullptr;

    // Состояние перетаскивания документа
    int m_dragDocRow = -1;
    int m_dragDocCol = -1;
    bool m_isDragging = false;
    QPoint m_dragStartPos;
};

#endif // SCREENCONFIGDIALOG_H
