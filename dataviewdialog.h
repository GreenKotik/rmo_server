#ifndef DATAVIEWDIALOG_H
#define DATAVIEWDIALOG_H

#include <QDialog>
#include <QVector>
#include <QMap>
#include <QVariant>
#include <QSet>
#include <QString>
#include <QJsonObject>
#include "qcustomplot.h"
#include "measurementsmodel.h"   // ✅ ПОЛНЫЙ ЗАГОЛОВОК (вместо forward)
#include "progressdialog.h"      // ✅ НОВЫЙ ДИАЛОГ ПРОГРЕССА

class DataTableProxy;
class JournalProxy;
class HighlightRowDelegate;
class QThread;
class DbLoaderWorker;

namespace Ui {
class DataViewDialog;
}

class DataViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DataViewDialog(const QString &filePath, int docType, QWidget *parent = nullptr);
    ~DataViewDialog();
    HighlightRowDelegate *m_rowHighlighter = nullptr;

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onLoadDataClicked();
    void onExportClicked();
    void onTreeItemChanged(QTreeWidgetItem *item, int column);

    void onGraphModeToggled();
    void onGraphSliderMoved(int value);

    void onGraphMousePress(QMouseEvent *event);
    void onGraphMouseMove(QMouseEvent *event);
    void onGraphMouseRelease(QMouseEvent *event);
    void onTableScrolled(int value);

    // ✅ слоты для работы с рабочим потоком
    void onDbLoadProgress(int rowsRead);
    void onDbLoadTotalRows(int totalRows);   // ✅ НОВЫЙ
    void onDbLoadFinished(const QStringList &columns,
                          const QVector<MeasurementsModel::Row> &rows,
                          int timeColumnIndex,
                          int errorColumnIndex,
                          const QVector<QPair<double, double>> &columnRanges);
    void onDbLoadFailed(const QString &errorMessage);

private:
    void setupGraph();
    void setupTreeChannels();
    void setupModels();
    void setupConnections();

    void loadDataFromDB();

    void updateGraph();
    void buildGraphs();
    QList<QString> getCheckedColumns() const;

    void syncVisibleColumnsFromTree();

    QString getSafeTableName(const QString &filePath);
    int rowIndexFromPixelX(int pixelX) const;

    void drawMarkers();
    void resetMarkers(bool doReplot = true);   // ← был без параметра

    Ui::DataViewDialog *ui;
    QString m_filePath;
    int m_docType;

    MeasurementsModel *m_model = nullptr;
    DataTableProxy    *m_dataProxy = nullptr;
    JournalProxy      *m_journalProxy = nullptr;

    int m_timeColumnIndex = -1;
    int m_errorColumnIndex = -1;

    bool m_detailedMode = false;
    static constexpr int DETAILED_ROWS = 50;
    static constexpr int DETAILED_TOP_OFFSET = 5;

    QCPItemLine *m_markerLine = nullptr;
    QCPItemLine *m_rangeStartLine = nullptr;
    QCPItemLine *m_rangeEndLine = nullptr;
    QCPItemRect *m_rangeRect = nullptr;
    QList<QCPItemLine*> m_dayBoundaryLines;

    bool m_selecting = false;
    QPoint m_pressPos;
    int m_markerRow = -1;
    int m_rangeStartRow = -1;
    int m_rangeEndRow = -1;
    bool m_rangingRange = false;
    bool m_syncingScroll = false;
    // Прокрутить таблицы к последней строке только при первом открытии
    bool m_firstLoad = true;

    QTime m_timeStart;
    QTime m_timeEnd;

    // Глобальный Y-диапазон по всей выборке (для фиксированной высоты графика)
    double m_yMin = 0.0;
    double m_yMax = 1.0;
    bool   m_yRangeValid = false;   // true — диапазон рассчитан, можно применять

    bool m_updatingTree = false;
    void applyCheckStateToChildren(QTreeWidgetItem *parent, Qt::CheckState state);
    void updateParentCheckState(QTreeWidgetItem *parent);

    QString treeStateFilePath() const;
    void saveTreeState();
    void restoreTreeState();

    void rebuildDayBoundaryLines();
    void updateTimeAxisFormat();

    // Текущий формат оси времени (для избежания лишних replot)
    QString m_currentTimeFormat = QStringLiteral("dd.MM hh:mm");
    // Пересчитать глобальный Y-диапазон по всей модели и отмеченным каналам
    void recalcGlobalYRange();
    void applyColumnWidths();
    // Создать leaf-элемент дерева: отображаемое имя + имя в БД
    QTreeWidgetItem* makeChannelItem(QTreeWidgetItem *parent, const QString &dbName);
    void moveErrorsColumnToEnd();
    // Пересчитать высоты ТОЛЬКО аварийных строк в tableData
    void recalcRowHeightsTableData();
    QJsonObject loadDbConfigFromFile();

    void startAsyncLoad();   // запускает воркер
    QJsonObject m_dbConfig;             // конфиг БД для воркера
    QThread *m_loaderThread = nullptr;
    DbLoaderWorker *m_loaderWorker = nullptr;
    bool m_loadingActive = false;
    // ✅ НОВОЕ: диалог прогресса с кнопкой "Отмена"
    ProgressDialog *m_progressDialog = nullptr;
    // ✅ НОВОЕ: вспомогательный метод — обновить прогресс и обработать события.
    // Возвращает false, если пользователь нажал "Отмена".
    bool updateProgressAndProcess(const QString &phase, int value = -1);
    // ✅ Закрыть диалог прогресса (не удаляя — переиспользуем).
    void closeProgressDialog();
    // ✅ Уничтожить диалог прогресса (вызывается из деструктора).
    void destroyProgressDialog();    // ✅ Прогресс-бар
    int m_totalRowsToLoad = 0;   // общее кол-во строк (из COUNT(*))
    QVector<int> m_expandedRows;
    // ✅ Min/max по каждому столбцу (из DbLoaderWorker), индекс = sourceColumn
    QVector<QPair<double, double>> m_columnRanges;
    // ✅ Подсветка сегодняшней даты в попапах обоих QDateEdit
    void updateCalendarHighlights();
};

#endif // DATAVIEWDIALOG_H
