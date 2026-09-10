#ifndef DATAVIEWDIALOG_H
#define DATAVIEWDIALOG_H

#include <QDialog>
#include <QVector>
#include "qcustomplot.h"

namespace Ui {
class DataViewDialog;
}

class DataViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DataViewDialog(const QString &filePath, int docType, QWidget *parent = nullptr);
    ~DataViewDialog();

private slots:
    void onLoadDataClicked();
    void onExportClicked();
    void onTreeItemChanged(QTreeWidgetItem *item, int column);

private:
    void setupGraph();
    void setupTreeChannels();
    void loadDataFromDB();
    void updateGraph();
    QString getSafeTableName(const QString &filePath);

    Ui::DataViewDialog *ui;
    QString m_filePath;
    int m_docType;
};

#endif // DATAVIEWDIALOG_H
