#include "measurementsmodel.h"
#include "alarmregistry.h"
#include "columnheaders.h"
#include <QColor>      // ✅ ДОБАВИТЬ ЭТУ СТРОКУ
#include <QDebug>
#include <cmath>   // ✅ для std::isnan

MeasurementsModel::MeasurementsModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int MeasurementsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_rows.size();
}

int MeasurementsModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_columnNames.size();
}

QVariant MeasurementsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};
    if (index.row() < 0 || index.row() >= m_rows.size()) return {};
    if (index.column() < 0 || index.column() >= m_columnNames.size()) return {};

    const Row &row = m_rows[index.row()];

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole: {
        const QString &colName = m_columnNames[index.column()];
        if (colName == "Дата_и_время") {
            return row.timestamp.toString("dd.MM.yyyy hh:mm:ss");
        }

        const QVariant value = row.values.value(index.column());

        // ✅ Обработка NaN: показываем "--.-" вместо "nan"
        if (value.canConvert<double>() && value.type() != QVariant::String) {
            bool ok = false;
            const double d = value.toDouble(&ok);
            if (ok && std::isnan(d)) {
                return QStringLiteral("--.-");
            }
        }

        return value;
    }
    case Qt::TextAlignmentRole:
        // Числа выравниваем вправо, строки — влево
        return QVariant(Qt::AlignRight | Qt::AlignVCenter);
    case Qt::ForegroundRole: {
        // Если это столбец с авариями и есть аварии — красный
        if (index.column() == m_errorColumn && rowHasErrors(index.row())) {
            return QColor(Qt::red);
        }
        return {};
    }
    default:
        return {};
    }
}

QVariant MeasurementsModel::headerData(int section, Qt::Orientation orientation,
                                       int role) const
{
    if (role != Qt::DisplayRole) return {};
    if (orientation == Qt::Horizontal) {
        const QString dbName = m_columnNames.value(section);
        return ColumnHeaders::displayName(dbName);
    }
    // Вертикальные заголовки — номера строк
    return section + 1;
}

Qt::ItemFlags MeasurementsModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void MeasurementsModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_rowErrorCounts.clear();   // ← добавить
    endResetModel();
}

void MeasurementsModel::setColumnNames(const QStringList &names)
{
    beginResetModel();
    m_columnNames = names;
    endResetModel();
}

void MeasurementsModel::appendRow(const Row &row)
{
    m_rows.append(row);

    // ✅ Парсим ErrorsAll один раз и запоминаем количество аварий
    int count = 0;
    const QString s = row.errorsString;
    if (!s.isEmpty()) {
        const QStringList parts = s.split(';', Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            bool ok = false;
            const int n = parts.first().toInt(&ok);
            if (ok) count = n;
        }
    }
    m_rowErrorCounts.append(count);
}

void MeasurementsModel::beginBulkInsert()
{
    beginResetModel();
    m_rows.clear();
    m_rowErrorCounts.clear();   // ← добавить
}

void MeasurementsModel::endBulkInsert()
{
    endResetModel();
}

int MeasurementsModel::columnIndex(const QString &name) const
{
    return m_columnNames.indexOf(name);
}

const MeasurementsModel::Row *MeasurementsModel::rowAt(int index) const
{
    if (index < 0 || index >= m_rows.size()) return nullptr;
    return &m_rows[index];
}

// ============================================================================
// ✅ Парсинг строки аварий (формат MFC: "N;code1{info};code2;...")
// ============================================================================
QStringList MeasurementsModel::rowErrorMessages(int row) const
{
    QStringList result;
    if (row < 0 || row >= m_rows.size()) return result;
    const QString s = m_rows[row].errorsString;
    if (s.isEmpty()) return result;

    const QStringList parts = s.split(';', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return result;

    bool ok = false;
    int count = parts.first().toInt(&ok);
    if (!ok) return result;

    for (int i = 1; i < parts.size(); ++i) {
        QString piece = parts[i].trimmed();
        if (piece.isEmpty()) continue;

        QString extraInfo;
        const int b1 = piece.indexOf('{');
        const int b2 = piece.indexOf('}');
        if (b1 != -1 && b2 != -1 && b1 < b2) {
            extraInfo = piece.mid(b1 + 1, b2 - b1 - 1);
            piece = piece.left(b1);
        }

        bool codeOk = false;
        const int code = piece.toInt(&codeOk);
        if (!codeOk) continue;

        QString msg = AlarmRegistry::instance().getMessage(code);
        if (!extraInfo.isEmpty())
            msg = extraInfo + " " + msg;

        result << msg;
    }
    return result;
}

int MeasurementsModel::rowErrorCount(int row) const
{
    return rowErrorCountCached(row);
}

bool MeasurementsModel::rowHasErrors(int row) const
{
    return rowErrorCount(row) > 0;
}
