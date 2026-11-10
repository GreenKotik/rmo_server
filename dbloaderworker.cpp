#include "dbloaderworker.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDebug>
#include <cmath>      // для std::isnan / std::isinf
#include <limits>     // для std::numeric_limits

// ============================================================================
// КОНСТРУКТОР / ДЕСТРУКТОР
// ============================================================================

DbLoaderWorker::DbLoaderWorker(const QJsonObject &dbConfig, QObject *parent)
    : QObject(parent)
    , m_dbConfig(dbConfig)
{
    // Уникальное имя соединения, привязанное к адресу this.
    // Это гарантирует отсутствие конфликтов между несколькими воркерами.
    m_connectionName = QString("dbloader_%1")
        .arg(reinterpret_cast<quintptr>(this));
}

DbLoaderWorker::~DbLoaderWorker()
{
    // Закрываем и удаляем соединение при уничтожении воркера.
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
            if (db.isOpen()) db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

// ============================================================================
// УПРАВЛЕНИЕ
// ============================================================================

void DbLoaderWorker::setLoadParams(const QString &tableName,
                                   const QDateTime &startDt,
                                   const QDateTime &endDt)
{
    m_tableName = tableName;
    m_startDt   = startDt;
    m_endDt     = endDt;
}

void DbLoaderWorker::cancel()
{
    m_cancelled.storeRelaxed(1);
}

// ============================================================================
// ОТКРЫТИЕ БД (в рабочем потоке — своё соединение)
// ============================================================================

bool DbLoaderWorker::openDatabase()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", m_connectionName);
    db.setHostName(m_dbConfig.value("host").toString("localhost"));
    db.setPort(m_dbConfig.value("port").toInt(5432));
    db.setDatabaseName(m_dbConfig.value("dbName").toString("rmo_db"));
    db.setUserName(m_dbConfig.value("user").toString("postgres"));
    db.setPassword(m_dbConfig.value("password").toString());

    if (!db.open()) {
        qWarning() << "[DbLoaderWorker] Не удалось открыть БД:"
                   << db.lastError().text();
        return false;
    }
    return true;
}

// ============================================================================
// ЗАГРУЗКА
// ============================================================================

void DbLoaderWorker::load()
{
    // Сброс флага отмены в начале загрузки
    m_cancelled.storeRelaxed(0);
    m_totalRows = 0;

    // Валидация параметров
    if (m_tableName.isEmpty() ||
        !m_startDt.isValid() || !m_endDt.isValid()) {
        emit failed("Параметры загрузки не установлены");
        return;
    }

    if (!openDatabase()) {
        emit failed("Не удалось подключиться к базе данных");
        return;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);

    // ------------------------------------------------------------------------
    // ШАГ 1: Считаем общее количество строк (для прогресс-бара)
    // ------------------------------------------------------------------------
    {
        QSqlQuery countQuery(db);
        countQuery.prepare(QString(
            "SELECT COUNT(*) FROM \"%1\" "
            "WHERE \"Дата_и_время\" >= :start "
            "  AND \"Дата_и_время\" <= :end").arg(m_tableName));
        countQuery.bindValue(":start", m_startDt);
        countQuery.bindValue(":end",   m_endDt);

        if (!countQuery.exec() || !countQuery.next()) {
            emit failed("Ошибка подсчёта строк: "
                        + countQuery.lastError().text());
            return;
        }

        m_totalRows = countQuery.value(0).toInt();
        qDebug() << "[DbLoaderWorker] Всего строк в диапазоне:" << m_totalRows;

        // Сообщаем главному потоку общее количество строк
        emit totalRowsKnown(m_totalRows);

        // Если строк нет — сразу выходим (пустой результат)
        if (m_totalRows == 0) {
            emit progress(0);
            emit finished(QStringList(),
                          QVector<MeasurementsModel::Row>(),
                          -1, -1,
                          QVector<QPair<double, double>>());
            return;
        }
    }

    // ------------------------------------------------------------------------
    // ШАГ 2: Основной запрос — читаем строки
    // ------------------------------------------------------------------------
    QSqlQuery query(db);
    query.prepare(QString(
        "SELECT * FROM \"%1\" "
        "WHERE \"Дата_и_время\" >= :start "
        "  AND \"Дата_и_время\" <= :end "
        "ORDER BY \"Дата_и_время\" ASC").arg(m_tableName));
    query.bindValue(":start", m_startDt);
    query.bindValue(":end",   m_endDt);

    if (!query.exec()) {
        emit failed("Ошибка запроса: " + query.lastError().text());
        return;
    }

    const QSqlRecord record = query.record();
    QStringList columns;
    columns.reserve(record.count());
    for (int i = 0; i < record.count(); ++i) {
        columns << record.fieldName(i);
    }

    const int timeCol = columns.indexOf("Дата_и_время");
    const int errCol  = columns.indexOf("ErrorsAll");

    if (timeCol < 0 || errCol < 0) {
        emit failed("В таблице нет ожидаемых столбцов "
                    "(Дата_и_время или ErrorsAll)");
        return;
    }

    QVector<MeasurementsModel::Row> rows;
    // ✅ Инициализируем min/max для каждого столбца
    //    Маркер «нет данных»: first > second
    m_columnRanges.clear();
    m_columnRanges.resize(columns.size());
    for (int i = 0; i < columns.size(); ++i) {
        m_columnRanges[i] = qMakePair(
            std::numeric_limits<double>::max(),
            std::numeric_limits<double>::lowest());
    }
    // Резервируем сразу нужный объём (мы знаем m_totalRows)
    rows.reserve(m_totalRows);

    // ✅ Адаптивный шаг эмита прогресса: не чаще 1000 раз за загрузку,
    //    но не реже, чем каждые 100 строк.
    const int progressStep = qMax(100, m_totalRows / 1000);

    int count = 0;
    while (query.next()) {
        if (m_cancelled.loadRelaxed()) {
            emit failed("Загрузка отменена пользователем");
            return;
        }

        MeasurementsModel::Row row;
        row.timestamp = query.value(timeCol).toDateTime();
        row.values.reserve(columns.size());
        for (int i = 0; i < columns.size(); ++i) {
            row.values.append(query.value(i));
        }
        row.errorsString = query.value(errCol).toString();
        rows.append(row);

        // ✅ Обновляем min/max для каждого числового столбца
        for (int i = 0; i < columns.size(); ++i) {
            const QVariant v = row.values.value(i);
            if (!v.isValid()) continue;

            bool ok = false;
            double d = v.toDouble(&ok);
            if (!ok) {
                d = v.toString().toDouble(&ok);
                if (!ok) continue;  // не число — пропускаем
            }
            if (std::isnan(d) || std::isinf(d)) continue;

            auto &range = m_columnRanges[i];
            if (d < range.first)  range.first  = d;
            if (d > range.second) range.second = d;
        }

        ++count;
        if (count % progressStep == 0) {
            emit progress(count);
        }
    }

    qDebug() << "[DbLoaderWorker] Загружено строк:" << count;

    // Финальный прогресс (на случай, если последний шаг не попал)
    emit progress(count);
    emit finished(columns, rows, timeCol, errCol, m_columnRanges);
}
