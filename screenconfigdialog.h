#ifndef SCREENCONFIGDIALOG_H
#define SCREENCONFIGDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QTableWidget>
#include <QMouseEvent>
#include <QEvent>
#include <QModelIndex>
#include <QHeaderView>
#include <QApplication>

namespace Ui {
    class ScreenConfigDialog;
}

class ScreenConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ScreenConfigDialog(int columns, const QStringList &docPaths, QWidget *parent = nullptr);
    ~ScreenConfigDialog();

    int columns() const;
    QStringList docPaths() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onColumnsChanged(int newCols);

private:
    Ui::ScreenConfigDialog *ui;
    void rebuildTable(int cols, const QStringList &paths);

    // Состояние перетаскивания
    int m_dragStartRow = -1;
    int m_dragStartCol = -1;
    bool m_isDragging = false;
    QPoint m_dragStartPos;
};

#endif // SCREENCONFIGDIALOG_H
