// measurementsmodel.h
#ifndef MEASUREMENTSMODEL_H
#define MEASUREMENTSMODEL_H

#include <QAbstractTableModel>
#include <QDateTime>
#include <QVector>
#include <QStringList>
#include <QVariant>
#include <QMap>


// ============================================================================
// ✅ ЕДИНАЯ МОДЕЛЬ ДАННЫХ ДЛЯ DataViewDialog
// Хранит все строки, полученные одним SELECT-запросом.
// Отдаёт данные через стандартный интерфейс QAbstractTableModel.
// ============================================================================
class MeasurementsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    // Одна строка данных
public:
    struct Row {
        QDateTime timestamp;
        QVector<QVariant> values;
        QString errorsString;
    };

    explicit MeasurementsModel(QObject *parent = nullptr);

    // --- QAbstractTableModel ---
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // --- Управление данными ---
    void clear();
    void setColumnNames(const QStringList &names);
    void appendRow(const Row &row);
    void beginBulkInsert();
    void endBulkInsert();

    // --- Доступ ---
    const QStringList &columnNames() const { return m_columnNames; }
    int columnIndex(const QString &name) const;
    const Row *rowAt(int index) const;
    const QVector<Row> &rows() const { return m_rows; }

    // --- Специализированные методы для парсинга аварий ---
    // Возвращает true, если в строке есть аварии
    bool rowHasErrors(int row) const;
    // Возвращает список расшифрованных сообщений для строки
    QStringList rowErrorMessages(int row) const;
    // Возвращает количество аварий в строке
    int rowErrorCount(int row) const;
    // Быстрый доступ к количеству аварий по строке модели
    int rowErrorCountCached(int row) const {
        if (row < 0 || row >= m_rowErrorCounts.size()) return 0;
        return m_rowErrorCounts.at(row);
    }

    // --- Настройка колонки с авариями ---
    void setErrorColumn(int col) { m_errorColumn = col; }
    int errorColumn() const { return m_errorColumn; }

private:
    QVector<Row> m_rows;
    QStringList m_columnNames;
    int m_errorColumn = -1;   // индекс столбца ErrorsAll
    // ✅ Кэш количества аварий по строкам — чтобы не парсить строку каждый раз
    QVector<int> m_rowErrorCounts;
};

// ✅ ВНЕ класса: регистрируем Row как метатип Qt
Q_DECLARE_METATYPE(MeasurementsModel::Row)

#endif // MEASUREMENTSMODEL_H
