#ifndef DBLOADERWORKER_H
#define DBLOADERWORKER_H

#include <QObject>
#include <QDateTime>
#include <QStringList>
#include <QPair>
#include <QVector>
#include <QJsonObject>
#include <QAtomicInt>
#include "measurementsmodel.h"

// ============================================================================
// ✅ РАБОЧИЙ ПОТОК ДЛЯ ЗАГРУЗКИ ДАННЫХ ИЗ БД
//
// Использование:
//   1. worker->setLoadParams(tableName, startDt, endDt);
//   2. worker->moveToThread(thread);
//   3. connect(thread, &QThread::started, worker, &DbLoaderWorker::load);
//   4. thread->start();
//
// ВАЖНО: worker использует СВОЁ соединение QSqlDatabase
// (Qt SQL не потокобезопасен, соединение принадлежит потоку).
// ============================================================================
class DbLoaderWorker : public QObject
{
    Q_OBJECT
public:
    explicit DbLoaderWorker(const QJsonObject &dbConfig,
                            QObject *parent = nullptr);
    ~DbLoaderWorker() override;

    // Установить параметры загрузки.
    // ВАЖНО: вызывать ДО thread->start() (из главного потока).
    void setLoadParams(const QString &tableName,
                       const QDateTime &startDt,
                       const QDateTime &endDt);

    // Отмена загрузки из главного потока (потокобезопасно).
    void cancel();

public slots:
    // Точка входа — вызывается в рабочем потоке.
    void load();

signals:
    // Прогресс: сколько строк уже прочитано.
    void progress(int rowsRead);

    // ✅ Общее количество строк в диапазоне (после SELECT COUNT(*)).
    //    Эмитится ОДИН РАЗ перед началом чтения строк.
    void totalRowsKnown(int totalRows);

    // Успешная загрузка.
    // columnRanges — min/max по каждому столбцу (first = min, second = max).
    // Если в столбце не было чисел — first > second (маркер «пусто»).
    void finished(const QStringList &columns,
                  const QVector<MeasurementsModel::Row> &rows,
                  int timeColumnIndex,
                  int errorColumnIndex,
                  const QVector<QPair<double, double>> &columnRanges);
    // Ошибка.
    void failed(const QString &errorMessage);

private:
    bool openDatabase();

    QJsonObject m_dbConfig;
    QString m_connectionName;

    // Параметры загрузки
    QString m_tableName;
    QDateTime m_startDt;
    QDateTime m_endDt;

    // ✅ Флаг отмены — atomic (читается из рабочего потока)
    QAtomicInt m_cancelled{0};

    // ✅ Общее количество строк (из COUNT(*)) — для адаптивного шага прогресса
    int m_totalRows = 0;
    // ✅ Min/max по каждому столбцу (индекс = columnIndex в columns).
    //    Для нечисловых столбцов (или если не было ни одного числа) —
    //    маркер «пусто»: first > second.
    QVector<QPair<double, double>> m_columnRanges;
};

#endif // DBLOADERWORKER_H
