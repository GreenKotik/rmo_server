// dataviewproxies.h
#ifndef DATAVIEWPROXIES_H
#define DATAVIEWPROXIES_H

#include <QSortFilterProxyModel>
#include <QSet>

// ============================================================================
// ✅ ПРОКСИ ДЛЯ ТАБЛИЦЫ ДАННЫХ
// Показывает только выбранные пользователем столбцы (не ErrorsAll).
// При изменении набора столбцов — обновляется мгновенно.
// ============================================================================
class DataTableProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit DataTableProxy(QObject *parent = nullptr);

    // Какие столбцы исходной модели показывать (по индексам источника)
    void setVisibleColumns(const QSet<int> &columns);
    QSet<int> visibleColumns() const { return m_visible; }
    void clearVisibleColumns();

protected:
    bool filterAcceptsColumn(int sourceColumn, const QModelIndex &sourceParent) const override;

private:
    QSet<int> m_visible;   // пусто = показывать все
};

// ============================================================================
// ✅ ПРОКСИ ДЛЯ ЖУРНАЛА АВАРИЙ
// Показывает только столбцы "Дата_и_время" + "ErrorsAll".
// Фильтрует строки, где нет аварий.
// ============================================================================
class JournalProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit JournalProxy(QObject *parent = nullptr);

    void setTimeColumn(int col) { m_timeColumn = col; invalidateFilter(); }
    void setErrorColumn(int col) { m_errorColumn = col; invalidateFilter(); }
    void setVisibleColumns(const QSet<int> &columns);

    // ✅ ДОБАВИТЬ:
    int errorColumn() const { return m_errorColumn; }
    int timeColumn() const { return m_timeColumn; }
    QSet<int> visibleColumns() const { return m_visible; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool filterAcceptsColumn(int sourceColumn, const QModelIndex &sourceParent) const override;

private:
    int m_timeColumn = -1;
    int m_errorColumn = -1;
    QSet<int> m_visible;
};

#endif // DATAVIEWPROXIES_H
