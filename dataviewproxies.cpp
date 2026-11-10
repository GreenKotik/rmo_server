// dataviewproxies.cpp
#include "dataviewproxies.h"
#include "measurementsmodel.h"

// ============================================================================
// DataTableProxy
// ============================================================================
DataTableProxy::DataTableProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void DataTableProxy::setVisibleColumns(const QSet<int> &columns)
{
    m_visible = columns;
    invalidateFilter();
}

void DataTableProxy::clearVisibleColumns()
{
    m_visible.clear();
    invalidateFilter();
}

bool DataTableProxy::filterAcceptsColumn(int sourceColumn,
                                         const QModelIndex & /*sourceParent*/) const
{
    if (m_visible.isEmpty()) return true;
    return m_visible.contains(sourceColumn);
}

// ============================================================================
// JournalProxy
// ============================================================================
JournalProxy::JournalProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void JournalProxy::setVisibleColumns(const QSet<int> &columns)
{
    m_visible = columns;
    invalidateFilter();
}

bool JournalProxy::filterAcceptsRow(int sourceRow,
                                    const QModelIndex &sourceParent) const
{
    if (!sourceModel()) return false;
    if (m_errorColumn < 0) return true;

    // Достаём строку ErrorsAll
    const QModelIndex idx = sourceModel()->index(sourceRow, m_errorColumn, sourceParent);
    const QString s = sourceModel()->data(idx).toString().trimmed();

    if (s.isEmpty() || s == "0;") return false;

    // Парсим первую часть: количество аварий
    const QStringList parts = s.split(';', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;

    bool ok = false;
    const int count = parts.first().toInt(&ok);
    return ok && count > 0;
}

bool JournalProxy::filterAcceptsColumn(int sourceColumn,
                                       const QModelIndex & /*sourceParent*/) const
{
    if (m_visible.isEmpty()) return true;
    return m_visible.contains(sourceColumn);
}
