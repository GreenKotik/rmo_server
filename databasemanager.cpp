#include "databasemanager.h"
#include <QSqlQuery>
#include <QJsonObject>
#include <QDateTime>
#include <QtMath> // Для qPow


// ============================================================================
// ✅ ЕДИНАЯ ФУНКЦИЯ ОЧИСТКИ ИМЕНИ ТАБЛИЦЫ
// Превращает "Лидер-7 (Tx VPol).per" в "лидер_7_tx_vpol_per"
// ============================================================================
QString DatabaseManager::getSafeTableName(const QString &filePath)
{
    QString name = QFileInfo(filePath).fileName(); // Берем только имя с расширением
    name.replace(".", "_")
        .replace(" ", "_")
        .replace("-", "_")
        .replace("(", "_")
        .replace(")", "_")
        .replace("'", "_")
        .replace("\"", "_")
        .replace(",", "_")
        .replace(";", "_");
    name = name.toLower();

    return name;
}

// ============================================================================
// ✅ СОЗДАНИЕ ТАБЛИЦЫ
// ============================================================================
bool DatabaseManager::ensureDocumentTableExists(const QString &filePath, int docType)
{
    if (!m_db.isOpen()) {
        qWarning() << "[DB] База данных не открыта";
        return false;
    }

    QString safeTableName = getSafeTableName(filePath);
    QString quotedTableName = "\"" + safeTableName + "\""; // Для безопасного использования в SQL

    // 1. Проверяем существование
    QSqlQuery checkQuery(m_db);
    checkQuery.prepare("SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name = ?)");
    checkQuery.addBindValue(safeTableName); // Здесь передаем БЕЗ кавычек для сравнения

    if (!checkQuery.exec() || !checkQuery.next()) {
        qWarning() << "[DB] Ошибка проверки таблицы:" << checkQuery.lastError().text();
        return false;
    }

    if (checkQuery.value(0).toBool()) {
        qDebug() << "[DB] Таблица" << safeTableName << "уже существует.";
        return true;
    }

    // 2. Создаем таблицу
    QSqlQuery createQuery(m_db);
    QString createSql;

    if (docType == 1) { // Антенна
        createSql = QString("CREATE TABLE %1 ("
            "id SERIAL PRIMARY KEY, \"Дата_и_время\" TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
            "\"Азимут\" REAL, \"Угол_места\" REAL, \"Угол_поляризации\" REAL, \"Уровень_сигнала\" REAL, "
            "\"Спутник\" VARCHAR(50), \"Статус\" VARCHAR(50), \"ОбогревТормозов\" SMALLINT, \"ПитаниеПриводов\" SMALLINT, "
            "\"AntErrors\" INTEGER, \"ErrorsAll\" TEXT, \"Komanda\" VARCHAR(100), "
            "\"ProtokolKontroller\" TEXT, \"ProtokolPilot\" TEXT, \"ProtokolDatAzimut\" TEXT, \"ProtokolDatUgol\" TEXT, "
            "\"ProtokolPosAzimut\" TEXT, \"ProtokolPosUgol\" TEXT, "
            "\"IBP1_V\" REAL, \"IBP1_A1\" REAL, \"IBP1_W1\" REAL, \"IBP1_A2\" REAL, \"IBP1_W2\" REAL, \"IBP1_A3\" REAL, \"IBP1_W3\" REAL, "
            "\"IBP1_A4\" REAL, \"IBP1_W4\" REAL, \"IBP1_A5\" REAL, \"IBP1_W5\" REAL, \"IBP1_A6\" REAL, \"IBP1_W6\" REAL, "
            "\"IBP2_V\" REAL, \"IBP2_A1\" REAL, \"IBP2_W1\" REAL, \"IBP2_A2\" REAL, \"IBP2_W2\" REAL, \"IBP2_A3\" REAL, \"IBP2_W3\" REAL, "
            "\"IBP2_A4\" REAL, \"IBP2_W4\" REAL, \"IBP2_A5\" REAL, \"IBP2_W5\" REAL, \"IBP2_A6\" REAL, \"IBP2_W6\" REAL, "
            "\"AV_V1\" REAL, \"AV_V2\" REAL, \"AV_V3\" REAL, \"AV_V4\" REAL, \"AV_V5\" REAL, \"AV_V6\" REAL, \"AV_V7\" REAL, \"AV_V8\" REAL, "
            "\"DV_D1\" SMALLINT, \"DV_D2\" SMALLINT, \"DV_D3\" SMALLINT, \"DV_D4\" SMALLINT, \"DV_D5\" SMALLINT, \"DV_D6\" SMALLINT, "
            "\"DV_D7\" SMALLINT, \"DV_D8\" SMALLINT, \"DV_D9\" SMALLINT, \"DV_D10\" SMALLINT, \"DV_D11\" SMALLINT, \"DV_D12\" SMALLINT, "
            "\"DV_D13\" SMALLINT, \"DV_D14\" SMALLINT, \"DV_D15\" SMALLINT, \"DV_D16\" SMALLINT, "
            "\"Temperatura1\" REAL, \"Temperatura2\" REAL, \"Temperatura3\" REAL, \"Temperatura4\" REAL, "
            "\"Temperatura5\" REAL, \"Temperatura6\" REAL, \"Temperatura7\" REAL, \"Temperatura8\" REAL, "
            "\"Obogrev_V1\" REAL, \"Obogrev_A1\" REAL, \"Obogrev_W1\" REAL, \"Obogrev_V2\" REAL, \"Obogrev_A2\" REAL, \"Obogrev_W2\" REAL, "
            "\"Obogrev_V3\" REAL, \"Obogrev_A3\" REAL, \"Obogrev_W3\" REAL, \"Obogrev_V4\" REAL, \"Obogrev_A4\" REAL, \"Obogrev_W4\" REAL, "
            "\"Obogrev_V5\" REAL, \"Obogrev_A5\" REAL, \"Obogrev_W5\" REAL, \"Obogrev_V6\" REAL, \"Obogrev_A6\" REAL, \"Obogrev_W6\" REAL, "
            "\"Obogrev_V7\" REAL, \"Obogrev_A7\" REAL, \"Obogrev_W7\" REAL, \"Obogrev_V8\" REAL, \"Obogrev_A8\" REAL, \"Obogrev_W8\" REAL, "
            "\"Obogrev_V9\" REAL, \"Obogrev_A9\" REAL, \"Obogrev_W9\" REAL, \"Obogrev_V10\" REAL, \"Obogrev_A10\" REAL, \"Obogrev_W10\" REAL, "
            "\"Obogrev_V11\" REAL, \"Obogrev_A11\" REAL, \"Obogrev_W11\" REAL, \"Obogrev_V12\" REAL, \"Obogrev_A12\" REAL, \"Obogrev_W12\" REAL"
            ")").arg(quotedTableName);
    }
    else if (docType == 2) { // Передатчик
        createSql = QString("CREATE TABLE %1 ("
            "id SERIAL PRIMARY KEY, \"Дата_и_время\" TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
            "\"Спутник\" VARCHAR(50), "
            "\"PerPowerIn1\" REAL, \"PerPower1\" REAL, \"PerPowerOtr1\" REAL, \"Attenuator1\" REAL, \"Spiral1\" REAL, \"PerStatus1\" INTEGER, "
            "\"PerPowerIn2\" REAL, \"PerPower2\" REAL, \"PerPowerOtr2\" REAL, \"Attenuator2\" REAL, \"Spiral2\" REAL, \"PerStatus2\" INTEGER, "
            "\"PerPowerIn3\" REAL, \"PerPower3\" REAL, \"PerPowerOtr3\" REAL, \"Attenuator3\" REAL, \"Spiral3\" REAL, \"PerStatus3\" INTEGER, "
            "\"SwitchPower\" REAL, \"SwitchStatus\" INTEGER, \"SwitchReg\" INTEGER, \"SwitchKanal\" INTEGER, "
            "\"Temper1\" REAL, \"Temper2\" REAL, "
            "\"ErrorsAll\" TEXT, \"ProtokolPer1\" TEXT, \"ProtokolPer2\" TEXT, \"ProtokolPer3\" TEXT, \"ProtokolPerSW\" TEXT"
            ")").arg(quotedTableName);
    }
    else if (docType == 3) { // Энерго
        createSql = QString("CREATE TABLE %1 ("
            "id SERIAL PRIMARY KEY, \"Дата_и_время\" TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
            "\"VV1_In_V1\" REAL, \"VV1_In_V2\" REAL, \"VV1_In_V3\" REAL, \"VV1_In_A1\" REAL, \"VV1_In_A2\" REAL, \"VV1_In_A3\" REAL, "
            "\"VV1_In_W1\" REAL, \"VV1_In_W2\" REAL, \"VV1_In_W3\" REAL, \"VV1_In_H\" REAL, \"VV1_In_AAll\" REAL, "
            "\"VV1_Out_V1\" REAL, \"VV1_Out_V2\" REAL, \"VV1_Out_V3\" REAL, \"VV1_Out_A1\" REAL, \"VV1_Out_A2\" REAL, \"VV1_Out_A3\" REAL, "
            "\"VV1_Out_W1\" REAL, \"VV1_Out_W2\" REAL, \"VV1_Out_W3\" REAL, \"VV1_Out_H\" REAL, \"VV1_Out_AAll\" REAL, "
            "\"IBP1_In_H\" REAL, \"IBP1_In_V1\" REAL, \"IBP1_In_V2\" REAL, \"IBP1_In_V3\" REAL, \"IBP1_In_A1\" REAL, \"IBP1_In_A2\" REAL, \"IBP1_In_A3\" REAL, "
            "\"IBP1_In_W1\" REAL, \"IBP1_In_W2\" REAL, \"IBP1_In_W3\" REAL, \"IBP1_Bypass_H\" REAL, \"IBP1_Bypass_V1\" REAL, \"IBP1_Bypass_V2\" REAL, \"IBP1_Bypass_V3\" REAL, "
            "\"IBP1_Out_H\" REAL, \"IBP1_Out_V1\" REAL, \"IBP1_Out_V2\" REAL, \"IBP1_Out_V3\" REAL, \"IBP1_Out_A1\" REAL, \"IBP1_Out_A2\" REAL, \"IBP1_Out_A3\" REAL, "
            "\"IBP1_Out_W1\" REAL, \"IBP1_Out_W2\" REAL, \"IBP1_Out_W3\" REAL, \"IBP1_Bat_V\" REAL, \"IBP1_Bat_A\" REAL, \"IBP1_Bat_Zarad\" REAL, "
            "\"Temperatura1\" REAL, \"Temperatura2\" REAL, \"ErrorsAll\" TEXT, \"ProtokolIBP1\" TEXT, \"ProtokolIBP2\" TEXT, \"ProtokolIBP3\" TEXT"
            ")").arg(quotedTableName);
    }
    else if (docType == 4) { // Модулятор
        createSql = QString("CREATE TABLE %1 ("
            "id SERIAL PRIMARY KEY, \"Дата_и_время\" TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
            "\"Статус\" VARCHAR(50), \"ErrorsAll\" TEXT"
            ")").arg(quotedTableName);
    }
    else {
        qWarning() << "[DB] Неизвестный тип документа:" << docType;
        return false;
    }

    if (!createQuery.exec(createSql)) {
        qWarning() << "[DB] Ошибка создания таблицы" << safeTableName << ":" << createQuery.lastError().text();
        return false;
    }

    qDebug() << "[DB] Таблица" << safeTableName << "успешно создана.";
    return true;
}


void DatabaseManager::migrateTableIfNeeded(const QString &safeTableName, int docType)
{
    if (!m_db.isOpen()) return;

    // Добавления полей
    // Список полей, которые должны быть в таблице
    // Формат: "имя_поля" → "ТИП ДАННЫХ"
    QMap<QString, QString> requiredFields;

    if (docType == 1) { // Антенна
//        requiredFields["\"Азимут\""] = "REAL";
//        requiredFields["\"Угол_места\""] = "REAL";
//        requiredFields["\"Спутник\""] = "VARCHAR(50)";
//        requiredFields["\"Новое_Поле\""] = "REAL";  // ✅ НОВОЕ ПОЛЕ
    }
    else if (docType == 2) { // Передатчик
//        requiredFields["\"Спутник\""] = "VARCHAR(50)";
//        requiredFields["\"PerPowerIn1\""] = "REAL";
//        requiredFields["\"Новое_Поле\""] = "REAL";  // ✅ НОВОЕ ПОЛЕ
    }

    // Проверяем каждое поле
    for (auto it = requiredFields.constBegin(); it != requiredFields.constEnd(); ++it) {
        QString fieldName = it.key();
        QString fieldType = it.value();

        // Проверяем, существует ли поле
        QSqlQuery checkField(m_db);
        checkField.prepare(
            "SELECT EXISTS ("
            "  SELECT FROM information_schema.columns "
            "  WHERE table_name = ? AND column_name = ?"
            ")"
        );
        checkField.addBindValue(safeTableName);
        // Убираем кавычки для сравнения с information_schema
        QString cleanFieldName = fieldName;
        cleanFieldName.replace("\"", "");
        checkField.addBindValue(cleanFieldName);

        if (checkField.exec() && checkField.next()) {
            bool fieldExists = checkField.value(0).toBool();

            if (!fieldExists) {
                // ✅ Добавляем отсутствующее поле
                QSqlQuery alterQuery(m_db);
                QString alterSql = QString("ALTER TABLE \"%1\" ADD COLUMN %2 %3")
                    .arg(safeTableName)
                    .arg(fieldName)
                    .arg(fieldType);

                if (alterQuery.exec(alterSql)) {
                    qDebug() << "[DB] ✅ Добавлено поле" << fieldName
                             << "в таблицу" << safeTableName;
                } else {
                    qWarning() << "[DB] ❌ Ошибка добавления поля" << fieldName
                               << ":" << alterQuery.lastError().text();
                }
            }
        }
    }

    // ✅ УДАЛЕНИЕ устаревших полей
    QStringList fieldsToRemove;

    if (docType == 1) {
//        fieldsToRemove << "\"СтароеНеиспользуемоеПоле\"";
//        fieldsToRemove << "\"Новое_Поле\"";
//        fieldsToRemove << "\"НовоеПоле\"";
    }
    else if (docType == 2) { // Передатчик
//        fieldsToRemove << "\"Новое_Поле\"";
//        fieldsToRemove << "\"НовоеПоле\"";
    }

    for (const QString &fieldName : fieldsToRemove) {
        // Сначала проверяем, что поле вообще существует
        QSqlQuery checkField(m_db);
        checkField.prepare(
            "SELECT EXISTS ("
            "  SELECT FROM information_schema.columns "
            "  WHERE table_name = ? AND column_name = ?"
            ")"
        );
        checkField.addBindValue(safeTableName);
        QString cleanFieldName = fieldName;
        cleanFieldName.replace("\"", "");
        checkField.addBindValue(cleanFieldName);

        if (checkField.exec() && checkField.next() && checkField.value(0).toBool()) {
            QSqlQuery dropQuery(m_db);
            QString dropSql = QString("ALTER TABLE \"%1\" DROP COLUMN %2")
                .arg(safeTableName)
                .arg(fieldName);

            if (dropQuery.exec(dropSql)) {
                qDebug() << "[DB] ✅ Удалено поле" << fieldName
                         << "из таблицы" << safeTableName;
            } else {
                qWarning() << "[DB] ❌ Ошибка удаления поля" << fieldName
                           << ":" << dropQuery.lastError().text();
            }
        }
    }
}

/*
bool DatabaseManager::logDocumentData(const QString &tableName, int docType, const QJsonObject &data)
{
    if (!m_db.isOpen()) {
        qWarning() << "[DB Log] База данных не открыта";
        return false;
    }

    QSqlQuery query(m_db);
    QString sql;

    // Вспомогательные лямбды для безопасного извлечения данных из JSON
    auto getDouble = [&](const QString &key) { return data.value(key).toDouble(0.0); };
    auto getInt = [&](const QString &key) { return data.value(key).toInt(0); };
    auto getString = [&](const QString &key) {
        QString val = data.value(key).toString("");
        // Санитизация протоколов как в MFC: обрезаем и заменяем одинарные кавычки
        if (key.startsWith("Protokol")) {
            val = val.left(253).replace("'", "_");
        }
        return val;
    };

    switch (docType) {
    case 1: { // Антенна (docType 1)
        sql = QString("INSERT INTO \"%1\" (\"Дата_и_время\", "
                      "\"Азимут\", \"Угол_места\", \"Угол_поляризации\", \"Уровень_сигнала\", "
                      "\"Спутник\", \"Статус\", \"ОбогревТормозов\", \"ПитаниеПриводов\", \"AntErrors\", \"ErrorsAll\", "
                      "\"Komanda\", \"ProtokolKontroller\", \"ProtokolPilot\", \"ProtokolDatAzimut\", \"ProtokolDatUgol\", "
                      "\"ProtokolPosAzimut\", \"ProtokolPosUgol\", "
                      "\"IBP1_V\", \"IBP1_A1\", \"IBP1_W1\", \"IBP1_A2\", \"IBP1_W2\", \"IBP1_A3\", \"IBP1_W3\", "
                      "\"IBP1_A4\", \"IBP1_W4\", \"IBP1_A5\", \"IBP1_W5\", \"IBP1_A6\", \"IBP1_W6\", "
                      "\"IBP2_V\", \"IBP2_A1\", \"IBP2_W1\", \"IBP2_A2\", \"IBP2_W2\", \"IBP2_A3\", \"IBP2_W3\", "
                      "\"IBP2_A4\", \"IBP2_W4\", \"IBP2_A5\", \"IBP2_W5\", \"IBP2_A6\", \"IBP2_W6\", "
                      "\"AV_V1\", \"AV_V2\", \"AV_V3\", \"AV_V4\", \"AV_V5\", \"AV_V6\", \"AV_V7\", \"AV_V8\", "
                      "\"DV_D1\", \"DV_D2\", \"DV_D3\", \"DV_D4\", \"DV_D5\", \"DV_D6\", \"DV_D7\", \"DV_D8\", "
                      "\"DV_D9\", \"DV_D10\", \"DV_D11\", \"DV_D12\", \"DV_D13\", \"DV_D14\", \"DV_D15\", \"DV_D16\", "
                      "\"Temperatura1\", \"Temperatura2\", \"Temperatura3\", \"Temperatura4\", "
                      "\"Temperatura5\", \"Temperatura6\", \"Temperatura7\", \"Temperatura8\", "
                      "\"Obogrev_V1\", \"Obogrev_A1\", \"Obogrev_W1\", \"Obogrev_V2\", \"Obogrev_A2\", \"Obogrev_W2\", "
                      "\"Obogrev_V3\", \"Obogrev_A3\", \"Obogrev_W3\", \"Obogrev_V4\", \"Obogrev_A4\", \"Obogrev_W4\", "
                      "\"Obogrev_V5\", \"Obogrev_A5\", \"Obogrev_W5\", \"Obogrev_V6\", \"Obogrev_A6\", \"Obogrev_W6\", "
                      "\"Obogrev_V7\", \"Obogrev_A7\", \"Obogrev_W7\", \"Obogrev_V8\", \"Obogrev_A8\", \"Obogrev_W8\", "
                      "\"Obogrev_V9\", \"Obogrev_A9\", \"Obogrev_W9\", \"Obogrev_V10\", \"Obogrev_A10\", \"Obogrev_W10\", "
                      "\"Obogrev_V11\", \"Obogrev_A11\", \"Obogrev_W11\", \"Obogrev_V12\", \"Obogrev_A12\", \"Obogrev_W12\") "
                      "VALUES (CURRENT_TIMESTAMP, "
                      ":az, :el, :pol, :sig, :sat, :stat, :obr_t, :pit_p, :ant_err, :err_all, "
                      ":kom, :pr_k, :pr_p, :pr_da, :pr_de, :pr_pa, :pr_pe, "
                      ":i1v, :i1a1, :i1w1, :i1a2, :i1w2, :i1a3, :i1w3, :i1a4, :i1w4, :i1a5, :i1w5, :i1a6, :i1w6, "
                      ":i2v, :i2a1, :i2w1, :i2a2, :i2w2, :i2a3, :i2w3, :i2a4, :i2w4, :i2a5, :i2w5, :i2a6, :i2w6, "
                      ":av1, :av2, :av3, :av4, :av5, :av6, :av7, :av8, "
                      ":d1, :d2, :d3, :d4, :d5, :d6, :d7, :d8, :d9, :d10, :d11, :d12, :d13, :d14, :d15, :d16, "
                      ":t1, :t2, :t3, :t4, :t5, :t6, :t7, :t8, "
                      ":ov1, :oa1, :ow1, :ov2, :oa2, :ow2, :ov3, :oa3, :ow3, :ov4, :oa4, :ow4, "
                      ":ov5, :oa5, :ow5, :ov6, :oa6, :ow6, :ov7, :oa7, :ow7, :ov8, :oa8, :ow8, "
                      ":ov9, :oa9, :ow9, :ov10, :oa10, :ow10, :ov11, :oa11, :ow11, :ov12, :oa12, :ow12)")
              .arg(tableName);

        query.prepare(sql);
        query.bindValue(":az", getDouble("Азимут"));
        query.bindValue(":el", getDouble("Угол_места"));
        query.bindValue(":pol", getDouble("Угол_поляризации"));
        query.bindValue(":sig", getDouble("Уровень_сигнала"));
        query.bindValue(":sat", getString("Спутник"));
        query.bindValue(":stat", getString("Статус"));
        query.bindValue(":obr_t", getInt("ОбогревТормозов"));
        query.bindValue(":pit_p", getInt("ПитаниеПриводов"));
        query.bindValue(":ant_err", getInt("AntErrors"));
        query.bindValue(":err_all", getString("ErrorsAll"));
        query.bindValue(":kom", getString("Komanda"));
        query.bindValue(":pr_k", getString("ProtokolKontroller"));
        query.bindValue(":pr_p", getString("ProtokolPilot"));
        query.bindValue(":pr_da", getString("ProtokolDatAzimut"));
        query.bindValue(":pr_de", getString("ProtokolDatUgol"));
        query.bindValue(":pr_pa", getString("ProtokolPosAzimut"));
        query.bindValue(":pr_pe", getString("ProtokolPosUgol"));

        // ИБП 1
        query.bindValue(":i1v", getDouble("IBP1_V")); query.bindValue(":i1a1", getDouble("IBP1_A1")); query.bindValue(":i1w1", getDouble("IBP1_W1"));
        query.bindValue(":i1a2", getDouble("IBP1_A2")); query.bindValue(":i1w2", getDouble("IBP1_W2")); query.bindValue(":i1a3", getDouble("IBP1_A3"));
        query.bindValue(":i1w3", getDouble("IBP1_W3")); query.bindValue(":i1a4", getDouble("IBP1_A4")); query.bindValue(":i1w4", getDouble("IBP1_W4"));
        query.bindValue(":i1a5", getDouble("IBP1_A5")); query.bindValue(":i1w5", getDouble("IBP1_W5")); query.bindValue(":i1a6", getDouble("IBP1_A6"));
        query.bindValue(":i1w6", getDouble("IBP1_W6"));
        // ИБП 2
        query.bindValue(":i2v", getDouble("IBP2_V")); query.bindValue(":i2a1", getDouble("IBP2_A1")); query.bindValue(":i2w1", getDouble("IBP2_W1"));
        query.bindValue(":i2a2", getDouble("IBP2_A2")); query.bindValue(":i2w2", getDouble("IBP2_W2")); query.bindValue(":i2a3", getDouble("IBP2_A3"));
        query.bindValue(":i2w3", getDouble("IBP2_W3")); query.bindValue(":i2a4", getDouble("IBP2_A4")); query.bindValue(":i2w4", getDouble("IBP2_W4"));
        query.bindValue(":i2a5", getDouble("IBP2_A5")); query.bindValue(":i2w5", getDouble("IBP2_W5")); query.bindValue(":i2a6", getDouble("IBP2_A6"));
        query.bindValue(":i2w6", getDouble("IBP2_W6"));
        // АВР
        query.bindValue(":av1", getDouble("AV_V1")); query.bindValue(":av2", getDouble("AV_V2")); query.bindValue(":av3", getDouble("AV_V3"));
        query.bindValue(":av4", getDouble("AV_V4")); query.bindValue(":av5", getDouble("AV_V5")); query.bindValue(":av6", getDouble("AV_V6"));
        query.bindValue(":av7", getDouble("AV_V7")); query.bindValue(":av8", getDouble("AV_V8"));
        // Дискреты
        for (int i = 1; i <= 16; ++i) {
            query.bindValue(QString(":d%1").arg(i), getInt(QString("DV_D%1").arg(i)));
        }
        // Температуры
        for (int i = 1; i <= 8; ++i) {
            query.bindValue(QString(":t%1").arg(i), getDouble(QString("Temperatura%1").arg(i)));
        }
        // Обогрев (пример для первых 3-х, добавьте остальные по аналогии)
        query.bindValue(":ov1", getDouble("Obogrev_V1")); query.bindValue(":oa1", getDouble("Obogrev_A1")); query.bindValue(":ow1", getDouble("Obogrev_W1"));
        query.bindValue(":ov2", getDouble("Obogrev_V2")); query.bindValue(":oa2", getDouble("Obogrev_A2")); query.bindValue(":ow2", getDouble("Obogrev_W2"));
        // ... (добавьте bindValue для Obogrev_V3..W12 по аналогии)

        break;
    }

    case 2: { // Передатчик (docType 2)
        // Примечание: В MFC была логика пересчета dBm в Ватты: pow(10, (dPower-30)/10).
        // Рекомендуется выполнять этот расчет в драйвере/виджете и передавать в JSON уже готовое значение "PerPower1".
        // Если же в JSON приходят "сырые" данные, расчет можно сделать здесь:
        double rawP1 = getDouble("RawPerPower1");
        double calcP1 = (rawP1 != 0.0) ? qPow(10.0, (rawP1 - 30.0) / 10.0) : getDouble("PerPower1");

        sql = QString("INSERT INTO \"%1\" (\"Дата_и_время\", \"Спутник\", "
                      "\"PerPowerIn1\", \"PerPower1\", \"PerPowerOtr1\", \"Attenuator1\", \"Spiral1\", \"PerStatus1\", "
                      "\"PerPowerIn2\", \"PerPower2\", \"PerPowerOtr2\", \"Attenuator2\", \"Spiral2\", \"PerStatus2\", "
                      "\"PerPowerIn3\", \"PerPower3\", \"PerPowerOtr3\", \"Attenuator3\", \"Spiral3\", \"PerStatus3\", "
                      "\"SwitchPower\", \"SwitchStatus\", \"SwitchReg\", \"SwitchKanal\", \"Temper1\", \"Temper2\", "
                      "\"ErrorsAll\", \"ProtokolPer1\", \"ProtokolPer2\", \"ProtokolPer3\", \"ProtokolPerSW\") "
                      "VALUES (CURRENT_TIMESTAMP, :sat, "
                      ":pi1, :p1, :po1, :a1, :s1, :st1, "
                      ":pi2, :p2, :po2, :a2, :s2, :st2, "
                      ":pi3, :p3, :po3, :a3, :s3, :st3, "
                      ":sw_p, :sw_st, :sw_r, :sw_k, :t1, :t2, "
                      ":err, :pr1, :pr2, :pr3, :prsw)")
              .arg(tableName);

        query.prepare(sql);
        query.bindValue(":sat", getString("Спутник"));

        // Канал 1
        query.bindValue(":pi1", getDouble("PerPowerIn1"));
        query.bindValue(":p1", calcP1); // Используем рассчитанное или готовое значение
        query.bindValue(":po1", getDouble("PerPowerOtr1"));
        query.bindValue(":a1", getDouble("Attenuator1"));
        query.bindValue(":s1", getDouble("Spiral1"));
        query.bindValue(":st1", getInt("PerStatus1"));

        // Канал 2 (аналогично)
        query.bindValue(":pi2", getDouble("PerPowerIn2"));
        query.bindValue(":p2", getDouble("PerPower2"));
        query.bindValue(":po2", getDouble("PerPowerOtr2"));
        query.bindValue(":a2", getDouble("Attenuator2"));
        query.bindValue(":s2", getDouble("Spiral2"));
        query.bindValue(":st2", getInt("PerStatus2"));

        // Канал 3 (аналогично)
        query.bindValue(":pi3", getDouble("PerPowerIn3"));
        query.bindValue(":p3", getDouble("PerPower3"));
        query.bindValue(":po3", getDouble("PerPowerOtr3"));
        query.bindValue(":a3", getDouble("Attenuator3"));
        query.bindValue(":s3", getDouble("Spiral3"));
        query.bindValue(":st3", getInt("PerStatus3"));

        // Переключатель и температуры
        query.bindValue(":sw_p", getDouble("SwitchPower"));
        query.bindValue(":sw_st", getInt("SwitchStatus"));
        query.bindValue(":sw_r", getInt("SwitchReg"));
        query.bindValue(":sw_k", getInt("SwitchKanal"));
        query.bindValue(":t1", getDouble("Temper1"));
        query.bindValue(":t2", getDouble("Temper2"));

        // Ошибки и протоколы
        query.bindValue(":err", getString("ErrorsAll"));
        query.bindValue(":pr1", getString("ProtokolPer1"));
        query.bindValue(":pr2", getString("ProtokolPer2"));
        query.bindValue(":pr3", getString("ProtokolPer3"));
        query.bindValue(":prsw", getString("ProtokolPerSW"));

        break;
    }

    case 3: { // Энерго (docType 3)
        sql = QString("INSERT INTO \"%1\" (\"Дата_и_время\", "
                      "\"VV1_In_V1\", \"VV1_In_V2\", \"VV1_In_V3\", \"VV1_In_A1\", \"VV1_In_A2\", \"VV1_In_A3\", "
                      "\"VV1_In_W1\", \"VV1_In_W2\", \"VV1_In_W3\", \"VV1_In_H\", \"VV1_In_AAll\", "
                      "\"VV1_Out_V1\", \"VV1_Out_V2\", \"VV1_Out_V3\", \"VV1_Out_A1\", \"VV1_Out_A2\", \"VV1_Out_A3\", "
                      "\"VV1_Out_W1\", \"VV1_Out_W2\", \"VV1_Out_W3\", \"VV1_Out_H\", \"VV1_Out_AAll\", "
                      "\"VV2_In_V1\", \"VV2_In_V2\", \"VV2_In_V3\", \"VV2_In_A1\", \"VV2_In_A2\", \"VV2_In_A3\", "
                      "\"VV2_In_W1\", \"VV2_In_W2\", \"VV2_In_W3\", \"VV2_In_H\", \"VV2_In_AAll\", "
                      "\"VV2_Out_V1\", \"VV2_Out_V2\", \"VV2_Out_V3\", \"VV2_Out_A1\", \"VV2_Out_A2\", \"VV2_Out_A3\", "
                      "\"VV2_Out_W1\", \"VV2_Out_W2\", \"VV2_Out_W3\", \"VV2_Out_H\", \"VV2_Out_AAll\", "
                      "\"VV3_In_V1\", \"VV3_In_V2\", \"VV3_In_V3\", \"VV3_In_A1\", \"VV3_In_A2\", \"VV3_In_A3\", "
                      "\"VV3_In_W1\", \"VV3_In_W2\", \"VV3_In_W3\", \"VV3_In_H\", \"VV3_In_AAll\", "
                      "\"VV3_Out_V1\", \"VV3_Out_V2\", \"VV3_Out_V3\", \"VV3_Out_A1\", \"VV3_Out_A2\", \"VV3_Out_A3\", "
                      "\"VV3_Out_W1\", \"VV3_Out_W2\", \"VV3_Out_W3\", \"VV3_Out_H\", \"VV3_Out_AAll\", "
                      "\"VV4_In_V1\", \"VV4_In_V2\", \"VV4_In_V3\", \"VV4_In_A1\", \"VV4_In_A2\", \"VV4_In_A3\", "
                      "\"VV4_In_W1\", \"VV4_In_W2\", \"VV4_In_W3\", \"VV4_In_H\", \"VV4_In_AAll\", "
                      "\"VV4_Out_V1\", \"VV4_Out_V2\", \"VV4_Out_V3\", \"VV4_Out_A1\", \"VV4_Out_A2\", \"VV4_Out_A3\", "
                      "\"VV4_Out_W1\", \"VV4_Out_W2\", \"VV4_Out_W3\", \"VV4_Out_H\", \"VV4_Out_AAll\", "
                      "\"IBP1_In_H\", \"IBP1_In_V1\", \"IBP1_In_V2\", \"IBP1_In_V3\", \"IBP1_In_A1\", \"IBP1_In_A2\", \"IBP1_In_A3\", "
                      "\"IBP1_In_W1\", \"IBP1_In_W2\", \"IBP1_In_W3\", \"IBP1_Bypass_H\", \"IBP1_Bypass_V1\", \"IBP1_Bypass_V2\", \"IBP1_Bypass_V3\", "
                      "\"IBP1_Out_H\", \"IBP1_Out_V1\", \"IBP1_Out_V2\", \"IBP1_Out_V3\", \"IBP1_Out_A1\", \"IBP1_Out_A2\", \"IBP1_Out_A3\", "
                      "\"IBP1_Out_W1\", \"IBP1_Out_W2\", \"IBP1_Out_W3\", \"IBP1_Bat_V\", \"IBP1_Bat_A\", \"IBP1_Bat_Zarad\", "
                      "\"IBP2_In_H\", \"IBP2_In_V1\", \"IBP2_In_V2\", \"IBP2_In_V3\", \"IBP2_In_A1\", \"IBP2_In_A2\", \"IBP2_In_A3\", "
                      "\"IBP2_In_W1\", \"IBP2_In_W2\", \"IBP2_In_W3\", \"IBP2_Bypass_H\", \"IBP2_Bypass_V1\", \"IBP2_Bypass_V2\", \"IBP2_Bypass_V3\", "
                      "\"IBP2_Out_H\", \"IBP2_Out_V1\", \"IBP2_Out_V2\", \"IBP2_Out_V3\", \"IBP2_Out_A1\", \"IBP2_Out_A2\", \"IBP2_Out_A3\", "
                      "\"IBP2_Out_W1\", \"IBP2_Out_W2\", \"IBP2_Out_W3\", \"IBP2_Bat_V\", \"IBP2_Bat_A\", \"IBP2_Bat_Zarad\", "
                      "\"IBP3_In_H\", \"IBP3_In_V1\", \"IBP3_In_V2\", \"IBP3_In_V3\", \"IBP3_In_A1\", \"IBP3_In_A2\", \"IBP3_In_A3\", "
                      "\"IBP3_In_W1\", \"IBP3_In_W2\", \"IBP3_In_W3\", \"IBP3_Bypass_H\", \"IBP3_Bypass_V1\", \"IBP3_Bypass_V2\", \"IBP3_Bypass_V3\", "
                      "\"IBP3_Out_H\", \"IBP3_Out_V1\", \"IBP3_Out_V2\", \"IBP3_Out_V3\", \"IBP3_Out_A1\", \"IBP3_Out_A2\", \"IBP3_Out_A3\", "
                      "\"IBP3_Out_W1\", \"IBP3_Out_W2\", \"IBP3_Out_W3\", \"IBP3_Bat_V\", \"IBP3_Bat_A\", \"IBP3_Bat_Zarad\", "
                      "\"Temperatura1\", \"Temperatura2\", \"ErrorsAll\", \"ProtokolIBP1\", \"ProtokolIBP2\", \"ProtokolIBP3\") "
                      "VALUES (CURRENT_TIMESTAMP, "
                      ":v1iv1, :v1iv2, :v1iv3, :v1ia1, :v1ia2, :v1ia3, :v1iw1, :v1iw2, :v1iw3, :v1ih, :v1iaa, "
                      ":v1ov1, :v1ov2, :v1ov3, :v1oa1, :v1oa2, :v1oa3, :v1ow1, :v1ow2, :v1ow3, :v1oh, :v1oaa, "
                      ":v2iv1, :v2iv2, :v2iv3, :v2ia1, :v2ia2, :v2ia3, :v2iw1, :v2iw2, :v2iw3, :v2ih, :v2iaa, "
                      ":v2ov1, :v2ov2, :v2ov3, :v2oa1, :v2oa2, :v2oa3, :v2ow1, :v2ow2, :v2ow3, :v2oh, :v2oaa, "
                      ":v3iv1, :v3iv2, :v3iv3, :v3ia1, :v3ia2, :v3ia3, :v3iw1, :v3iw2, :v3iw3, :v3ih, :v3iaa, "
                      ":v3ov1, :v3ov2, :v3ov3, :v3oa1, :v3oa2, :v3oa3, :v3ow1, :v3ow2, :v3ow3, :v3oh, :v3oaa, "
                      ":v4iv1, :v4iv2, :v4iv3, :v4ia1, :v4ia2, :v4ia3, :v4iw1, :v4iw2, :v4iw3, :v4ih, :v4iaa, "
                      ":v4ov1, :v4ov2, :v4ov3, :v4oa1, :v4oa2, :v4oa3, :v4ow1, :v4ow2, :v4ow3, :v4oh, :v4oaa, "
                      ":i1ih, :i1iv1, :i1iv2, :i1iv3, :i1ia1, :i1ia2, :i1ia3, :i1iw1, :i1iw2, :i1iw3, :i1bh, :i1bv1, :i1bv2, :i1bv3, "
                      ":i1oh, :i1ov1, :i1ov2, :i1ov3, :i1oa1, :i1oa2, :i1oa3, :i1ow1, :i1ow2, :i1ow3, :i1bv, :i1ba, :i1bz, "
                      ":i2ih, :i2iv1, :i2iv2, :i2iv3, :i2ia1, :i2ia2, :i2ia3, :i2iw1, :i2iw2, :i2iw3, :i2bh, :i2bv1, :i2bv2, :i2bv3, "
                      ":i2oh, :i2ov1, :i2ov2, :i2ov3, :i2oa1, :i2oa2, :i2oa3, :i2ow1, :i2ow2, :i2ow3, :i2bv, :i2ba, :i2bz, "
                      ":i3ih, :i3iv1, :i3iv2, :i3iv3, :i3ia1, :i3ia2, :i3ia3, :i3iw1, :i3iw2, :i3iw3, :i3bh, :i3bv1, :i3bv2, :i3bv3, "
                      ":i3oh, :i3ov1, :i3ov2, :i3ov3, :i3oa1, :i3oa2, :i3oa3, :i3ow1, :i3ow2, :i3ow3, :i3bv, :i3ba, :i3bz, "
                      ":t1, :t2, :err, :p1, :p2, :p3)")
              .arg(tableName);

        query.prepare(sql);

        // Вспомогательный макрос для краткости bindValue (все поля REAL/Double, кроме протоколов и ошибок)
        #define BIND_D(name, key) query.bindValue(name, getDouble(key))
        #define BIND_S(name, key) query.bindValue(name, getString(key))

        // VV1 In/Out
        BIND_D(":v1iv1", "VV1_In_V1"); BIND_D(":v1iv2", "VV1_In_V2"); BIND_D(":v1iv3", "VV1_In_V3");
        BIND_D(":v1ia1", "VV1_In_A1"); BIND_D(":v1ia2", "VV1_In_A2"); BIND_D(":v1ia3", "VV1_In_A3");
        BIND_D(":v1iw1", "VV1_In_W1"); BIND_D(":v1iw2", "VV1_In_W2"); BIND_D(":v1iw3", "VV1_In_W3");
        BIND_D(":v1ih", "VV1_In_H"); BIND_D(":v1iaa", "VV1_In_AAll");
        BIND_D(":v1ov1", "VV1_Out_V1"); BIND_D(":v1ov2", "VV1_Out_V2"); BIND_D(":v1ov3", "VV1_Out_V3");
        BIND_D(":v1oa1", "VV1_Out_A1"); BIND_D(":v1oa2", "VV1_Out_A2"); BIND_D(":v1oa3", "VV1_Out_A3");
        BIND_D(":v1ow1", "VV1_Out_W1"); BIND_D(":v1ow2", "VV1_Out_W2"); BIND_D(":v1ow3", "VV1_Out_W3");
        BIND_D(":v1oh", "VV1_Out_H"); BIND_D(":v1oaa", "VV1_Out_AAll");

        // VV2 In/Out
        BIND_D(":v2iv1", "VV2_In_V1"); BIND_D(":v2iv2", "VV2_In_V2"); BIND_D(":v2iv3", "VV2_In_V3");
        BIND_D(":v2ia1", "VV2_In_A1"); BIND_D(":v2ia2", "VV2_In_A2"); BIND_D(":v2ia3", "VV2_In_A3");
        BIND_D(":v2iw1", "VV2_In_W1"); BIND_D(":v2iw2", "VV2_In_W2"); BIND_D(":v2iw3", "VV2_In_W3");
        BIND_D(":v2ih", "VV2_In_H"); BIND_D(":v2iaa", "VV2_In_AAll");
        BIND_D(":v2ov1", "VV2_Out_V1"); BIND_D(":v2ov2", "VV2_Out_V2"); BIND_D(":v2ov3", "VV2_Out_V3");
        BIND_D(":v2oa1", "VV2_Out_A1"); BIND_D(":v2oa2", "VV2_Out_A2"); BIND_D(":v2oa3", "VV2_Out_A3");
        BIND_D(":v2ow1", "VV2_Out_W1"); BIND_D(":v2ow2", "VV2_Out_W2"); BIND_D(":v2ow3", "VV2_Out_W3");
        BIND_D(":v2oh", "VV2_Out_H"); BIND_D(":v2oaa", "VV2_Out_AAll");

        // VV3 In/Out
        BIND_D(":v3iv1", "VV3_In_V1"); BIND_D(":v3iv2", "VV3_In_V2"); BIND_D(":v3iv3", "VV3_In_V3");
        BIND_D(":v3ia1", "VV3_In_A1"); BIND_D(":v3ia2", "VV3_In_A2"); BIND_D(":v3ia3", "VV3_In_A3");
        BIND_D(":v3iw1", "VV3_In_W1"); BIND_D(":v3iw2", "VV3_In_W2"); BIND_D(":v3iw3", "VV3_In_W3");
        BIND_D(":v3ih", "VV3_In_H"); BIND_D(":v3iaa", "VV3_In_AAll");
        BIND_D(":v3ov1", "VV3_Out_V1"); BIND_D(":v3ov2", "VV3_Out_V2"); BIND_D(":v3ov3", "VV3_Out_V3");
        BIND_D(":v3oa1", "VV3_Out_A1"); BIND_D(":v3oa2", "VV3_Out_A2"); BIND_D(":v3oa3", "VV3_Out_A3");
        BIND_D(":v3ow1", "VV3_Out_W1"); BIND_D(":v3ow2", "VV3_Out_W2"); BIND_D(":v3ow3", "VV3_Out_W3");
        BIND_D(":v3oh", "VV3_Out_H"); BIND_D(":v3oaa", "VV3_Out_AAll");

        // VV4 In/Out
        BIND_D(":v4iv1", "VV4_In_V1"); BIND_D(":v4iv2", "VV4_In_V2"); BIND_D(":v4iv3", "VV4_In_V3");
        BIND_D(":v4ia1", "VV4_In_A1"); BIND_D(":v4ia2", "VV4_In_A2"); BIND_D(":v4ia3", "VV4_In_A3");
        BIND_D(":v4iw1", "VV4_In_W1"); BIND_D(":v4iw2", "VV4_In_W2"); BIND_D(":v4iw3", "VV4_In_W3");
        BIND_D(":v4ih", "VV4_In_H"); BIND_D(":v4iaa", "VV4_In_AAll");
        BIND_D(":v4ov1", "VV4_Out_V1"); BIND_D(":v4ov2", "VV4_Out_V2"); BIND_D(":v4ov3", "VV4_Out_V3");
        BIND_D(":v4oa1", "VV4_Out_A1"); BIND_D(":v4oa2", "VV4_Out_A2"); BIND_D(":v4oa3", "VV4_Out_A3");
        BIND_D(":v4ow1", "VV4_Out_W1"); BIND_D(":v4ow2", "VV4_Out_W2"); BIND_D(":v4ow3", "VV4_Out_W3");
        BIND_D(":v4oh", "VV4_Out_H"); BIND_D(":v4oaa", "VV4_Out_AAll");

        // ИБП 1
        BIND_D(":i1ih", "IBP1_In_H"); BIND_D(":i1iv1", "IBP1_In_V1"); BIND_D(":i1iv2", "IBP1_In_V2"); BIND_D(":i1iv3", "IBP1_In_V3");
        BIND_D(":i1ia1", "IBP1_In_A1"); BIND_D(":i1ia2", "IBP1_In_A2"); BIND_D(":i1ia3", "IBP1_In_A3");
        BIND_D(":i1iw1", "IBP1_In_W1"); BIND_D(":i1iw2", "IBP1_In_W2"); BIND_D(":i1iw3", "IBP1_In_W3");
        BIND_D(":i1bh", "IBP1_Bypass_H"); BIND_D(":i1bv1", "IBP1_Bypass_V1"); BIND_D(":i1bv2", "IBP1_Bypass_V2"); BIND_D(":i1bv3", "IBP1_Bypass_V3");
        BIND_D(":i1oh", "IBP1_Out_H"); BIND_D(":i1ov1", "IBP1_Out_V1"); BIND_D(":i1ov2", "IBP1_Out_V2"); BIND_D(":i1ov3", "IBP1_Out_V3");
        BIND_D(":i1oa1", "IBP1_Out_A1"); BIND_D(":i1oa2", "IBP1_Out_A2"); BIND_D(":i1oa3", "IBP1_Out_A3");
        BIND_D(":i1ow1", "IBP1_Out_W1"); BIND_D(":i1ow2", "IBP1_Out_W2"); BIND_D(":i1ow3", "IBP1_Out_W3");
        BIND_D(":i1bv", "IBP1_Bat_V"); BIND_D(":i1ba", "IBP1_Bat_A"); BIND_D(":i1bz", "IBP1_Bat_Zarad");

        // ИБП 2
        BIND_D(":i2ih", "IBP2_In_H"); BIND_D(":i2iv1", "IBP2_In_V1"); BIND_D(":i2iv2", "IBP2_In_V2"); BIND_D(":i2iv3", "IBP2_In_V3");
        BIND_D(":i2ia1", "IBP2_In_A1"); BIND_D(":i2ia2", "IBP2_In_A2"); BIND_D(":i2ia3", "IBP2_In_A3");
        BIND_D(":i2iw1", "IBP2_In_W1"); BIND_D(":i2iw2", "IBP2_In_W2"); BIND_D(":i2iw3", "IBP2_In_W3");
        BIND_D(":i2bh", "IBP2_Bypass_H"); BIND_D(":i2bv1", "IBP2_Bypass_V1"); BIND_D(":i2bv2", "IBP2_Bypass_V2"); BIND_D(":i2bv3", "IBP2_Bypass_V3");
        BIND_D(":i2oh", "IBP2_Out_H"); BIND_D(":i2ov1", "IBP2_Out_V1"); BIND_D(":i2ov2", "IBP2_Out_V2"); BIND_D(":i2ov3", "IBP2_Out_V3");
        BIND_D(":i2oa1", "IBP2_Out_A1"); BIND_D(":i2oa2", "IBP2_Out_A2"); BIND_D(":i2oa3", "IBP2_Out_A3");
        BIND_D(":i2ow1", "IBP2_Out_W1"); BIND_D(":i2ow2", "IBP2_Out_W2"); BIND_D(":i2ow3", "IBP2_Out_W3");
        BIND_D(":i2bv", "IBP2_Bat_V"); BIND_D(":i2ba", "IBP2_Bat_A"); BIND_D(":i2bz", "IBP2_Bat_Zarad");

        // ИБП 3
        BIND_D(":i3ih", "IBP3_In_H"); BIND_D(":i3iv1", "IBP3_In_V1"); BIND_D(":i3iv2", "IBP3_In_V2"); BIND_D(":i3iv3", "IBP3_In_V3");
        BIND_D(":i3ia1", "IBP3_In_A1"); BIND_D(":i3ia2", "IBP3_In_A2"); BIND_D(":i3ia3", "IBP3_In_A3");
        BIND_D(":i3iw1", "IBP3_In_W1"); BIND_D(":i3iw2", "IBP3_In_W2"); BIND_D(":i3iw3", "IBP3_In_W3");
        BIND_D(":i3bh", "IBP3_Bypass_H"); BIND_D(":i3bv1", "IBP3_Bypass_V1"); BIND_D(":i3bv2", "IBP3_Bypass_V2"); BIND_D(":i3bv3", "IBP3_Bypass_V3");
        BIND_D(":i3oh", "IBP3_Out_H"); BIND_D(":i3ov1", "IBP3_Out_V1"); BIND_D(":i3ov2", "IBP3_Out_V2"); BIND_D(":i3ov3", "IBP3_Out_V3");
        BIND_D(":i3oa1", "IBP3_Out_A1"); BIND_D(":i3oa2", "IBP3_Out_A2"); BIND_D(":i3oa3", "IBP3_Out_A3");
        BIND_D(":i3ow1", "IBP3_Out_W1"); BIND_D(":i3ow2", "IBP3_Out_W2"); BIND_D(":i3ow3", "IBP3_Out_W3");
        BIND_D(":i3bv", "IBP3_Bat_V"); BIND_D(":i3ba", "IBP3_Bat_A"); BIND_D(":i3bz", "IBP3_Bat_Zarad");

        // Температуры и общие поля
        BIND_D(":t1", "Temperatura1");
        BIND_D(":t2", "Temperatura2");
        BIND_S(":err", "ErrorsAll");
        BIND_S(":p1", "ProtokolIBP1");
        BIND_S(":p2", "ProtokolIBP2");
        BIND_S(":p3", "ProtokolIBP3");

        #undef BIND_D
        #undef BIND_S

        break;
    }

    case 4: { // Модулятор (docType 4)
        sql = QString("INSERT INTO \"%1\" (\"Дата_и_время\", \"Статус\", \"ErrorsAll\") "
                      "VALUES (CURRENT_TIMESTAMP, :stat, :err)").arg(tableName);
        query.prepare(sql);
        query.bindValue(":stat", getString("Статус"));
        query.bindValue(":err", getString("ErrorsAll"));
        // TODO: Добавьте поля модулятора, когда определите их список
        break;
    }

    default:
        qWarning() << "[DB Log] Неизвестный тип документа:" << docType;
        return false;
    }

    if (!query.exec()) {
        qWarning() << "[DB Log] Ошибка записи в таблицу" << tableName << ":" << query.lastError().text();
        return false;
    }

    return true;
}
*/

// ============================================================================
// ✅ ЗАПИСЬ ДАННЫХ В ТАБЛИЦУ
// ============================================================================
bool DatabaseManager::logDocumentData(const QString &filePath, int docType, const QJsonObject &data)
{
    if (!m_db.isOpen()) {
        qWarning() << "[DB Log] База данных не открыта";
        return false;
    }

    // ✅ ИСПОЛЬЗУЕМ ТУ ЖЕ ФУНКЦИЮ, ЧТО И ПРИ СОЗДАНИИ!
    QString safeTableName = getSafeTableName(filePath);
    QString quotedTableName = "\"" + safeTableName + "\"";

    QSqlQuery query(m_db);
    QString sql;

    // Вспомогательные лямбды для безопасного извлечения данных
    auto getDouble = [&](const QString &key) { return data.value(key).toDouble(0.0); };
    auto getInt = [&](const QString &key) { return data.value(key).toInt(0); };
    auto getString = [&](const QString &key) {
        QString val = data.value(key).toString("");
        if (key.startsWith("Protokol")) {
            val = val.left(253).replace("'", "_");
        }
        return val;
    };

    switch (docType) {
    case 1: { // Антенна
        sql = QString("INSERT INTO %1 (\"Дата_и_время\", "
                      "\"Азимут\", \"Угол_места\", \"Угол_поляризации\", \"Уровень_сигнала\", "
                      "\"Спутник\", \"Статус\", \"ОбогревТормозов\", \"ПитаниеПриводов\", \"AntErrors\", \"ErrorsAll\", "
                      "\"Komanda\", \"ProtokolKontroller\", \"ProtokolPilot\", \"ProtokolDatAzimut\", \"ProtokolDatUgol\", "
                      "\"ProtokolPosAzimut\", \"ProtokolPosUgol\", "
                      "\"IBP1_V\", \"IBP1_A1\", \"IBP1_W1\", \"IBP1_A2\", \"IBP1_W2\", \"IBP1_A3\", \"IBP1_W3\", "
                      "\"IBP1_A4\", \"IBP1_W4\", \"IBP1_A5\", \"IBP1_W5\", \"IBP1_A6\", \"IBP1_W6\", "
                      "\"IBP2_V\", \"IBP2_A1\", \"IBP2_W1\", \"IBP2_A2\", \"IBP2_W2\", \"IBP2_A3\", \"IBP2_W3\", "
                      "\"IBP2_A4\", \"IBP2_W4\", \"IBP2_A5\", \"IBP2_W5\", \"IBP2_A6\", \"IBP2_W6\", "
                      "\"AV_V1\", \"AV_V2\", \"AV_V3\", \"AV_V4\", \"AV_V5\", \"AV_V6\", \"AV_V7\", \"AV_V8\", "
                      "\"DV_D1\", \"DV_D2\", \"DV_D3\", \"DV_D4\", \"DV_D5\", \"DV_D6\", \"DV_D7\", \"DV_D8\", "
                      "\"DV_D9\", \"DV_D10\", \"DV_D11\", \"DV_D12\", \"DV_D13\", \"DV_D14\", \"DV_D15\", \"DV_D16\", "
                      "\"Temperatura1\", \"Temperatura2\", \"Temperatura3\", \"Temperatura4\", "
                      "\"Temperatura5\", \"Temperatura6\", \"Temperatura7\", \"Temperatura8\", "
                      "\"Obogrev_V1\", \"Obogrev_A1\", \"Obogrev_W1\", \"Obogrev_V2\", \"Obogrev_A2\", \"Obogrev_W2\", "
                      "\"Obogrev_V3\", \"Obogrev_A3\", \"Obogrev_W3\", \"Obogrev_V4\", \"Obogrev_A4\", \"Obogrev_W4\", "
                      "\"Obogrev_V5\", \"Obogrev_A5\", \"Obogrev_W5\", \"Obogrev_V6\", \"Obogrev_A6\", \"Obogrev_W6\", "
                      "\"Obogrev_V7\", \"Obogrev_A7\", \"Obogrev_W7\", \"Obogrev_V8\", \"Obogrev_A8\", \"Obogrev_W8\", "
                      "\"Obogrev_V9\", \"Obogrev_A9\", \"Obogrev_W9\", \"Obogrev_V10\", \"Obogrev_A10\", \"Obogrev_W10\", "
                      "\"Obogrev_V11\", \"Obogrev_A11\", \"Obogrev_W11\", \"Obogrev_V12\", \"Obogrev_A12\", \"Obogrev_W12\") "
                      "VALUES (CURRENT_TIMESTAMP, "
                      ":az, :el, :pol, :sig, :sat, :stat, :obr_t, :pit_p, :ant_err, :err_all, "
                      ":kom, :pr_k, :pr_p, :pr_da, :pr_de, :pr_pa, :pr_pe, "
                      ":i1v, :i1a1, :i1w1, :i1a2, :i1w2, :i1a3, :i1w3, :i1a4, :i1w4, :i1a5, :i1w5, :i1a6, :i1w6, "
                      ":i2v, :i2a1, :i2w1, :i2a2, :i2w2, :i2a3, :i2w3, :i2a4, :i2w4, :i2a5, :i2w5, :i2a6, :i2w6, "
                      ":av1, :av2, :av3, :av4, :av5, :av6, :av7, :av8, "
                      ":d1, :d2, :d3, :d4, :d5, :d6, :d7, :d8, :d9, :d10, :d11, :d12, :d13, :d14, :d15, :d16, "
                      ":t1, :t2, :t3, :t4, :t5, :t6, :t7, :t8, "
                      ":ov1, :oa1, :ow1, :ov2, :oa2, :ow2, :ov3, :oa3, :ow3, :ov4, :oa4, :ow4, "
                      ":ov5, :oa5, :ow5, :ov6, :oa6, :ow6, :ov7, :oa7, :ow7, :ov8, :oa8, :ow8, "
                      ":ov9, :oa9, :ow9, :ov10, :oa10, :ow10, :ov11, :oa11, :ow11, :ov12, :oa12, :ow12)")
              .arg(quotedTableName);

        query.prepare(sql);
        query.bindValue(":az", getDouble("Азимут"));
        query.bindValue(":el", getDouble("Угол_места"));
        query.bindValue(":pol", getDouble("Угол_поляризации"));
        query.bindValue(":sig", getDouble("Уровень_сигнала"));
        query.bindValue(":sat", getString("Спутник"));
        query.bindValue(":stat", getString("Статус"));
        query.bindValue(":obr_t", getInt("ОбогревТормозов"));
        query.bindValue(":pit_p", getInt("ПитаниеПриводов"));
        query.bindValue(":ant_err", getInt("AntErrors"));
        query.bindValue(":err_all", getString("ErrorsAll"));
        query.bindValue(":kom", getString("Komanda"));
        query.bindValue(":pr_k", getString("ProtokolKontroller"));
        query.bindValue(":pr_p", getString("ProtokolPilot"));
        query.bindValue(":pr_da", getString("ProtokolDatAzimut"));
        query.bindValue(":pr_de", getString("ProtokolDatUgol"));
        query.bindValue(":pr_pa", getString("ProtokolPosAzimut"));
        query.bindValue(":pr_pe", getString("ProtokolPosUgol"));

        query.bindValue(":i1v", getDouble("IBP1_V")); query.bindValue(":i1a1", getDouble("IBP1_A1")); query.bindValue(":i1w1", getDouble("IBP1_W1"));
        query.bindValue(":i1a2", getDouble("IBP1_A2")); query.bindValue(":i1w2", getDouble("IBP1_W2")); query.bindValue(":i1a3", getDouble("IBP1_A3"));
        query.bindValue(":i1w3", getDouble("IBP1_W3")); query.bindValue(":i1a4", getDouble("IBP1_A4")); query.bindValue(":i1w4", getDouble("IBP1_W4"));
        query.bindValue(":i1a5", getDouble("IBP1_A5")); query.bindValue(":i1w5", getDouble("IBP1_W5")); query.bindValue(":i1a6", getDouble("IBP1_A6"));
        query.bindValue(":i1w6", getDouble("IBP1_W6"));

        query.bindValue(":i2v", getDouble("IBP2_V")); query.bindValue(":i2a1", getDouble("IBP2_A1")); query.bindValue(":i2w1", getDouble("IBP2_W1"));
        query.bindValue(":i2a2", getDouble("IBP2_A2")); query.bindValue(":i2w2", getDouble("IBP2_W2")); query.bindValue(":i2a3", getDouble("IBP2_A3"));
        query.bindValue(":i2w3", getDouble("IBP2_W3")); query.bindValue(":i2a4", getDouble("IBP2_A4")); query.bindValue(":i2w4", getDouble("IBP2_W4"));
        query.bindValue(":i2a5", getDouble("IBP2_A5")); query.bindValue(":i2w5", getDouble("IBP2_W5")); query.bindValue(":i2a6", getDouble("IBP2_A6"));
        query.bindValue(":i2w6", getDouble("IBP2_W6"));

        query.bindValue(":av1", getDouble("AV_V1")); query.bindValue(":av2", getDouble("AV_V2")); query.bindValue(":av3", getDouble("AV_V3"));
        query.bindValue(":av4", getDouble("AV_V4")); query.bindValue(":av5", getDouble("AV_V5")); query.bindValue(":av6", getDouble("AV_V6"));
        query.bindValue(":av7", getDouble("AV_V7")); query.bindValue(":av8", getDouble("AV_V8"));

        for (int i = 1; i <= 16; ++i) {
            query.bindValue(QString(":d%1").arg(i), getInt(QString("DV_D%1").arg(i)));
        }

        for (int i = 1; i <= 8; ++i) {
            query.bindValue(QString(":t%1").arg(i), getDouble(QString("Temperatura%1").arg(i)));
        }

        query.bindValue(":ov1", getDouble("Obogrev_V1")); query.bindValue(":oa1", getDouble("Obogrev_A1")); query.bindValue(":ow1", getDouble("Obogrev_W1"));
        query.bindValue(":ov2", getDouble("Obogrev_V2")); query.bindValue(":oa2", getDouble("Obogrev_A2")); query.bindValue(":ow2", getDouble("Obogrev_W2"));
        query.bindValue(":ov3", getDouble("Obogrev_V3")); query.bindValue(":oa3", getDouble("Obogrev_A3")); query.bindValue(":ow3", getDouble("Obogrev_W3"));
        query.bindValue(":ov4", getDouble("Obogrev_V4")); query.bindValue(":oa4", getDouble("Obogrev_A4")); query.bindValue(":ow4", getDouble("Obogrev_W4"));
        query.bindValue(":ov5", getDouble("Obogrev_V5")); query.bindValue(":oa5", getDouble("Obogrev_A5")); query.bindValue(":ow5", getDouble("Obogrev_W5"));
        query.bindValue(":ov6", getDouble("Obogrev_V6")); query.bindValue(":oa6", getDouble("Obogrev_A6")); query.bindValue(":ow6", getDouble("Obogrev_W6"));
        query.bindValue(":ov7", getDouble("Obogrev_V7")); query.bindValue(":oa7", getDouble("Obogrev_A7")); query.bindValue(":ow7", getDouble("Obogrev_W7"));
        query.bindValue(":ov8", getDouble("Obogrev_V8")); query.bindValue(":oa8", getDouble("Obogrev_A8")); query.bindValue(":ow8", getDouble("Obogrev_W8"));
        query.bindValue(":ov9", getDouble("Obogrev_V9")); query.bindValue(":oa9", getDouble("Obogrev_A9")); query.bindValue(":ow9", getDouble("Obogrev_W9"));
        query.bindValue(":ov10", getDouble("Obogrev_V10")); query.bindValue(":oa10", getDouble("Obogrev_A10")); query.bindValue(":ow10", getDouble("Obogrev_W10"));
        query.bindValue(":ov11", getDouble("Obogrev_V11")); query.bindValue(":oa11", getDouble("Obogrev_A11")); query.bindValue(":ow11", getDouble("Obogrev_W11"));
        query.bindValue(":ov12", getDouble("Obogrev_V12")); query.bindValue(":oa12", getDouble("Obogrev_A12")); query.bindValue(":ow12", getDouble("Obogrev_W12"));

        break;
    }

    case 2: { // Передатчик
        sql = QString("INSERT INTO %1 (\"Дата_и_время\", \"Спутник\", "
                      "\"PerPowerIn1\", \"PerPower1\", \"PerPowerOtr1\", \"Attenuator1\", \"Spiral1\", \"PerStatus1\", "
                      "\"PerPowerIn2\", \"PerPower2\", \"PerPowerOtr2\", \"Attenuator2\", \"Spiral2\", \"PerStatus2\", "
                      "\"PerPowerIn3\", \"PerPower3\", \"PerPowerOtr3\", \"Attenuator3\", \"Spiral3\", \"PerStatus3\", "
                      "\"SwitchPower\", \"SwitchStatus\", \"SwitchReg\", \"SwitchKanal\", \"Temper1\", \"Temper2\", "
                      "\"ErrorsAll\", \"ProtokolPer1\", \"ProtokolPer2\", \"ProtokolPer3\", \"ProtokolPerSW\") "
                      "VALUES (CURRENT_TIMESTAMP, :sat, "
                      ":pi1, :p1, :po1, :a1, :s1, :st1, "
                      ":pi2, :p2, :po2, :a2, :s2, :st2, "
                      ":pi3, :p3, :po3, :a3, :s3, :st3, "
                      ":sw_p, :sw_st, :sw_r, :sw_k, :t1, :t2, "
                      ":err, :pr1, :pr2, :pr3, :prsw)")
              .arg(quotedTableName);

        query.prepare(sql);
        query.bindValue(":sat", getString("Спутник"));

        query.bindValue(":pi1", getDouble("PerPowerIn1"));
        query.bindValue(":p1", getDouble("PerPower1"));
        query.bindValue(":po1", getDouble("PerPowerOtr1"));
        query.bindValue(":a1", getDouble("Attenuator1"));
        query.bindValue(":s1", getDouble("Spiral1"));
        query.bindValue(":st1", getInt("PerStatus1"));

        query.bindValue(":pi2", getDouble("PerPowerIn2"));
        query.bindValue(":p2", getDouble("PerPower2"));
        query.bindValue(":po2", getDouble("PerPowerOtr2"));
        query.bindValue(":a2", getDouble("Attenuator2"));
        query.bindValue(":s2", getDouble("Spiral2"));
        query.bindValue(":st2", getInt("PerStatus2"));

        query.bindValue(":pi3", getDouble("PerPowerIn3"));
        query.bindValue(":p3", getDouble("PerPower3"));
        query.bindValue(":po3", getDouble("PerPowerOtr3"));
        query.bindValue(":a3", getDouble("Attenuator3"));
        query.bindValue(":s3", getDouble("Spiral3"));
        query.bindValue(":st3", getInt("PerStatus3"));

        query.bindValue(":sw_p", getDouble("SwitchPower"));
        query.bindValue(":sw_st", getInt("SwitchStatus"));
        query.bindValue(":sw_r", getInt("SwitchReg"));
        query.bindValue(":sw_k", getInt("SwitchKanal"));
        query.bindValue(":t1", getDouble("Temper1"));
        query.bindValue(":t2", getDouble("Temper2"));

        query.bindValue(":err", getString("ErrorsAll"));
        query.bindValue(":pr1", getString("ProtokolPer1"));
        query.bindValue(":pr2", getString("ProtokolPer2"));
        query.bindValue(":pr3", getString("ProtokolPer3"));
        query.bindValue(":prsw", getString("ProtokolPerSW"));

        break;
    }

    case 3: { // Энерго
        sql = QString("INSERT INTO %1 (\"Дата_и_время\", "
                      "\"VV1_In_V1\", \"VV1_In_V2\", \"VV1_In_V3\", \"VV1_In_A1\", \"VV1_In_A2\", \"VV1_In_A3\", "
                      "\"VV1_In_W1\", \"VV1_In_W2\", \"VV1_In_W3\", \"VV1_In_H\", \"VV1_In_AAll\", "
                      "\"VV1_Out_V1\", \"VV1_Out_V2\", \"VV1_Out_V3\", \"VV1_Out_A1\", \"VV1_Out_A2\", \"VV1_Out_A3\", "
                      "\"VV1_Out_W1\", \"VV1_Out_W2\", \"VV1_Out_W3\", \"VV1_Out_H\", \"VV1_Out_AAll\", "
                      "\"VV2_In_V1\", \"VV2_In_V2\", \"VV2_In_V3\", \"VV2_In_A1\", \"VV2_In_A2\", \"VV2_In_A3\", "
                      "\"VV2_In_W1\", \"VV2_In_W2\", \"VV2_In_W3\", \"VV2_In_H\", \"VV2_In_AAll\", "
                      "\"VV2_Out_V1\", \"VV2_Out_V2\", \"VV2_Out_V3\", \"VV2_Out_A1\", \"VV2_Out_A2\", \"VV2_Out_A3\", "
                      "\"VV2_Out_W1\", \"VV2_Out_W2\", \"VV2_Out_W3\", \"VV2_Out_H\", \"VV2_Out_AAll\", "
                      "\"VV3_In_V1\", \"VV3_In_V2\", \"VV3_In_V3\", \"VV3_In_A1\", \"VV3_In_A2\", \"VV3_In_A3\", "
                      "\"VV3_In_W1\", \"VV3_In_W2\", \"VV3_In_W3\", \"VV3_In_H\", \"VV3_In_AAll\", "
                      "\"VV3_Out_V1\", \"VV3_Out_V2\", \"VV3_Out_V3\", \"VV3_Out_A1\", \"VV3_Out_A2\", \"VV3_Out_A3\", "
                      "\"VV3_Out_W1\", \"VV3_Out_W2\", \"VV3_Out_W3\", \"VV3_Out_H\", \"VV3_Out_AAll\", "
                      "\"VV4_In_V1\", \"VV4_In_V2\", \"VV4_In_V3\", \"VV4_In_A1\", \"VV4_In_A2\", \"VV4_In_A3\", "
                      "\"VV4_In_W1\", \"VV4_In_W2\", \"VV4_In_W3\", \"VV4_In_H\", \"VV4_In_AAll\", "
                      "\"VV4_Out_V1\", \"VV4_Out_V2\", \"VV4_Out_V3\", \"VV4_Out_A1\", \"VV4_Out_A2\", \"VV4_Out_A3\", "
                      "\"VV4_Out_W1\", \"VV4_Out_W2\", \"VV4_Out_W3\", \"VV4_Out_H\", \"VV4_Out_AAll\", "
                      "\"IBP1_In_H\", \"IBP1_In_V1\", \"IBP1_In_V2\", \"IBP1_In_V3\", \"IBP1_In_A1\", \"IBP1_In_A2\", \"IBP1_In_A3\", "
                      "\"IBP1_In_W1\", \"IBP1_In_W2\", \"IBP1_In_W3\", \"IBP1_Bypass_H\", \"IBP1_Bypass_V1\", \"IBP1_Bypass_V2\", \"IBP1_Bypass_V3\", "
                      "\"IBP1_Out_H\", \"IBP1_Out_V1\", \"IBP1_Out_V2\", \"IBP1_Out_V3\", \"IBP1_Out_A1\", \"IBP1_Out_A2\", \"IBP1_Out_A3\", "
                      "\"IBP1_Out_W1\", \"IBP1_Out_W2\", \"IBP1_Out_W3\", \"IBP1_Bat_V\", \"IBP1_Bat_A\", \"IBP1_Bat_Zarad\", "
                      "\"IBP2_In_H\", \"IBP2_In_V1\", \"IBP2_In_V2\", \"IBP2_In_V3\", \"IBP2_In_A1\", \"IBP2_In_A2\", \"IBP2_In_A3\", "
                      "\"IBP2_In_W1\", \"IBP2_In_W2\", \"IBP2_In_W3\", \"IBP2_Bypass_H\", \"IBP2_Bypass_V1\", \"IBP2_Bypass_V2\", \"IBP2_Bypass_V3\", "
                      "\"IBP2_Out_H\", \"IBP2_Out_V1\", \"IBP2_Out_V2\", \"IBP2_Out_V3\", \"IBP2_Out_A1\", \"IBP2_Out_A2\", \"IBP2_Out_A3\", "
                      "\"IBP2_Out_W1\", \"IBP2_Out_W2\", \"IBP2_Out_W3\", \"IBP2_Bat_V\", \"IBP2_Bat_A\", \"IBP2_Bat_Zarad\", "
                      "\"IBP3_In_H\", \"IBP3_In_V1\", \"IBP3_In_V2\", \"IBP3_In_V3\", \"IBP3_In_A1\", \"IBP3_In_A2\", \"IBP3_In_A3\", "
                      "\"IBP3_In_W1\", \"IBP3_In_W2\", \"IBP3_In_W3\", \"IBP3_Bypass_H\", \"IBP3_Bypass_V1\", \"IBP3_Bypass_V2\", \"IBP3_Bypass_V3\", "
                      "\"IBP3_Out_H\", \"IBP3_Out_V1\", \"IBP3_Out_V2\", \"IBP3_Out_V3\", \"IBP3_Out_A1\", \"IBP3_Out_A2\", \"IBP3_Out_A3\", "
                      "\"IBP3_Out_W1\", \"IBP3_Out_W2\", \"IBP3_Out_W3\", \"IBP3_Bat_V\", \"IBP3_Bat_A\", \"IBP3_Bat_Zarad\", "
                      "\"Temperatura1\", \"Temperatura2\", \"ErrorsAll\", \"ProtokolIBP1\", \"ProtokolIBP2\", \"ProtokolIBP3\") "
                      "VALUES (CURRENT_TIMESTAMP, "
                      ":v1iv1, :v1iv2, :v1iv3, :v1ia1, :v1ia2, :v1ia3, :v1iw1, :v1iw2, :v1iw3, :v1ih, :v1iaa, "
                      ":v1ov1, :v1ov2, :v1ov3, :v1oa1, :v1oa2, :v1oa3, :v1ow1, :v1ow2, :v1ow3, :v1oh, :v1oaa, "
                      ":v2iv1, :v2iv2, :v2iv3, :v2ia1, :v2ia2, :v2ia3, :v2iw1, :v2iw2, :v2iw3, :v2ih, :v2iaa, "
                      ":v2ov1, :v2ov2, :v2ov3, :v2oa1, :v2oa2, :v2oa3, :v2ow1, :v2ow2, :v2ow3, :v2oh, :v2oaa, "
                      ":v3iv1, :v3iv2, :v3iv3, :v3ia1, :v3ia2, :v3ia3, :v3iw1, :v3iw2, :v3iw3, :v3ih, :v3iaa, "
                      ":v3ov1, :v3ov2, :v3ov3, :v3oa1, :v3oa2, :v3oa3, :v3ow1, :v3ow2, :v3ow3, :v3oh, :v3oaa, "
                      ":v4iv1, :v4iv2, :v4iv3, :v4ia1, :v4ia2, :v4ia3, :v4iw1, :v4iw2, :v4iw3, :v4ih, :v4iaa, "
                      ":v4ov1, :v4ov2, :v4ov3, :v4oa1, :v4oa2, :v4oa3, :v4ow1, :v4ow2, :v4ow3, :v4oh, :v4oaa, "
                      ":i1ih, :i1iv1, :i1iv2, :i1iv3, :i1ia1, :i1ia2, :i1ia3, :i1iw1, :i1iw2, :i1iw3, :i1bh, :i1bv1, :i1bv2, :i1bv3, "
                      ":i1oh, :i1ov1, :i1ov2, :i1ov3, :i1oa1, :i1oa2, :i1oa3, :i1ow1, :i1ow2, :i1ow3, :i1bv, :i1ba, :i1bz, "
                      ":i2ih, :i2iv1, :i2iv2, :i2iv3, :i2ia1, :i2ia2, :i2ia3, :i2iw1, :i2iw2, :i2iw3, :i2bh, :i2bv1, :i2bv2, :i2bv3, "
                      ":i2oh, :i2ov1, :i2ov2, :i2ov3, :i2oa1, :i2oa2, :i2oa3, :i2ow1, :i2ow2, :i2ow3, :i2bv, :i2ba, :i2bz, "
                      ":i3ih, :i3iv1, :i3iv2, :i3iv3, :i3ia1, :i3ia2, :i3ia3, :i3iw1, :i3iw2, :i3iw3, :i3bh, :i3bv1, :i3bv2, :i3bv3, "
                      ":i3oh, :i3ov1, :i3ov2, :i3ov3, :i3oa1, :i3oa2, :i3oa3, :i3ow1, :i3ow2, :i3ow3, :i3bv, :i3ba, :i3bz, "
                      ":t1, :t2, :err, :p1, :p2, :p3)")
              .arg(quotedTableName);

        query.prepare(sql);

        #define BIND_D(name, key) query.bindValue(name, getDouble(key))
        #define BIND_S(name, key) query.bindValue(name, getString(key))

        BIND_D(":v1iv1", "VV1_In_V1"); BIND_D(":v1iv2", "VV1_In_V2"); BIND_D(":v1iv3", "VV1_In_V3");
        BIND_D(":v1ia1", "VV1_In_A1"); BIND_D(":v1ia2", "VV1_In_A2"); BIND_D(":v1ia3", "VV1_In_A3");
        BIND_D(":v1iw1", "VV1_In_W1"); BIND_D(":v1iw2", "VV1_In_W2"); BIND_D(":v1iw3", "VV1_In_W3");
        BIND_D(":v1ih", "VV1_In_H"); BIND_D(":v1iaa", "VV1_In_AAll");
        BIND_D(":v1ov1", "VV1_Out_V1"); BIND_D(":v1ov2", "VV1_Out_V2"); BIND_D(":v1ov3", "VV1_Out_V3");
        BIND_D(":v1oa1", "VV1_Out_A1"); BIND_D(":v1oa2", "VV1_Out_A2"); BIND_D(":v1oa3", "VV1_Out_A3");
        BIND_D(":v1ow1", "VV1_Out_W1"); BIND_D(":v1ow2", "VV1_Out_W2"); BIND_D(":v1ow3", "VV1_Out_W3");
        BIND_D(":v1oh", "VV1_Out_H"); BIND_D(":v1oaa", "VV1_Out_AAll");

        BIND_D(":v2iv1", "VV2_In_V1"); BIND_D(":v2iv2", "VV2_In_V2"); BIND_D(":v2iv3", "VV2_In_V3");
        BIND_D(":v2ia1", "VV2_In_A1"); BIND_D(":v2ia2", "VV2_In_A2"); BIND_D(":v2ia3", "VV2_In_A3");
        BIND_D(":v2iw1", "VV2_In_W1"); BIND_D(":v2iw2", "VV2_In_W2"); BIND_D(":v2iw3", "VV2_In_W3");
        BIND_D(":v2ih", "VV2_In_H"); BIND_D(":v2iaa", "VV2_In_AAll");
        BIND_D(":v2ov1", "VV2_Out_V1"); BIND_D(":v2ov2", "VV2_Out_V2"); BIND_D(":v2ov3", "VV2_Out_V3");
        BIND_D(":v2oa1", "VV2_Out_A1"); BIND_D(":v2oa2", "VV2_Out_A2"); BIND_D(":v2oa3", "VV2_Out_A3");
        BIND_D(":v2ow1", "VV2_Out_W1"); BIND_D(":v2ow2", "VV2_Out_W2"); BIND_D(":v2ow3", "VV2_Out_W3");
        BIND_D(":v2oh", "VV2_Out_H"); BIND_D(":v2oaa", "VV2_Out_AAll");

        BIND_D(":v3iv1", "VV3_In_V1"); BIND_D(":v3iv2", "VV3_In_V2"); BIND_D(":v3iv3", "VV3_In_V3");
        BIND_D(":v3ia1", "VV3_In_A1"); BIND_D(":v3ia2", "VV3_In_A2"); BIND_D(":v3ia3", "VV3_In_A3");
        BIND_D(":v3iw1", "VV3_In_W1"); BIND_D(":v3iw2", "VV3_In_W2"); BIND_D(":v3iw3", "VV3_In_W3");
        BIND_D(":v3ih", "VV3_In_H"); BIND_D(":v3iaa", "VV3_In_AAll");
        BIND_D(":v3ov1", "VV3_Out_V1"); BIND_D(":v3ov2", "VV3_Out_V2"); BIND_D(":v3ov3", "VV3_Out_V3");
        BIND_D(":v3oa1", "VV3_Out_A1"); BIND_D(":v3oa2", "VV3_Out_A2"); BIND_D(":v3oa3", "VV3_Out_A3");
        BIND_D(":v3ow1", "VV3_Out_W1"); BIND_D(":v3ow2", "VV3_Out_W2"); BIND_D(":v3ow3", "VV3_Out_W3");
        BIND_D(":v3oh", "VV3_Out_H"); BIND_D(":v3oaa", "VV3_Out_AAll");

        BIND_D(":v4iv1", "VV4_In_V1"); BIND_D(":v4iv2", "VV4_In_V2"); BIND_D(":v4iv3", "VV4_In_V3");
        BIND_D(":v4ia1", "VV4_In_A1"); BIND_D(":v4ia2", "VV4_In_A2"); BIND_D(":v4ia3", "VV4_In_A3");
        BIND_D(":v4iw1", "VV4_In_W1"); BIND_D(":v4iw2", "VV4_In_W2"); BIND_D(":v4iw3", "VV4_In_W3");
        BIND_D(":v4ih", "VV4_In_H"); BIND_D(":v4iaa", "VV4_In_AAll");
        BIND_D(":v4ov1", "VV4_Out_V1"); BIND_D(":v4ov2", "VV4_Out_V2"); BIND_D(":v4ov3", "VV4_Out_V3");
        BIND_D(":v4oa1", "VV4_Out_A1"); BIND_D(":v4oa2", "VV4_Out_A2"); BIND_D(":v4oa3", "VV4_Out_A3");
        BIND_D(":v4ow1", "VV4_Out_W1"); BIND_D(":v4ow2", "VV4_Out_W2"); BIND_D(":v4ow3", "VV4_Out_W3");
        BIND_D(":v4oh", "VV4_Out_H"); BIND_D(":v4oaa", "VV4_Out_AAll");

        BIND_D(":i1ih", "IBP1_In_H"); BIND_D(":i1iv1", "IBP1_In_V1"); BIND_D(":i1iv2", "IBP1_In_V2"); BIND_D(":i1iv3", "IBP1_In_V3");
        BIND_D(":i1ia1", "IBP1_In_A1"); BIND_D(":i1ia2", "IBP1_In_A2"); BIND_D(":i1ia3", "IBP1_In_A3");
        BIND_D(":i1iw1", "IBP1_In_W1"); BIND_D(":i1iw2", "IBP1_In_W2"); BIND_D(":i1iw3", "IBP1_In_W3");
        BIND_D(":i1bh", "IBP1_Bypass_H"); BIND_D(":i1bv1", "IBP1_Bypass_V1"); BIND_D(":i1bv2", "IBP1_Bypass_V2"); BIND_D(":i1bv3", "IBP1_Bypass_V3");
        BIND_D(":i1oh", "IBP1_Out_H"); BIND_D(":i1ov1", "IBP1_Out_V1"); BIND_D(":i1ov2", "IBP1_Out_V2"); BIND_D(":i1ov3", "IBP1_Out_V3");
        BIND_D(":i1oa1", "IBP1_Out_A1"); BIND_D(":i1oa2", "IBP1_Out_A2"); BIND_D(":i1oa3", "IBP1_Out_A3");
        BIND_D(":i1ow1", "IBP1_Out_W1"); BIND_D(":i1ow2", "IBP1_Out_W2"); BIND_D(":i1ow3", "IBP1_Out_W3");
        BIND_D(":i1bv", "IBP1_Bat_V"); BIND_D(":i1ba", "IBP1_Bat_A"); BIND_D(":i1bz", "IBP1_Bat_Zarad");

        BIND_D(":i2ih", "IBP2_In_H"); BIND_D(":i2iv1", "IBP2_In_V1"); BIND_D(":i2iv2", "IBP2_In_V2"); BIND_D(":i2iv3", "IBP2_In_V3");
        BIND_D(":i2ia1", "IBP2_In_A1"); BIND_D(":i2ia2", "IBP2_In_A2"); BIND_D(":i2ia3", "IBP2_In_A3");
        BIND_D(":i2iw1", "IBP2_In_W1"); BIND_D(":i2iw2", "IBP2_In_W2"); BIND_D(":i2iw3", "IBP2_In_W3");
        BIND_D(":i2bh", "IBP2_Bypass_H"); BIND_D(":i2bv1", "IBP2_Bypass_V1"); BIND_D(":i2bv2", "IBP2_Bypass_V2"); BIND_D(":i2bv3", "IBP2_Bypass_V3");
        BIND_D(":i2oh", "IBP2_Out_H"); BIND_D(":i2ov1", "IBP2_Out_V1"); BIND_D(":i2ov2", "IBP2_Out_V2"); BIND_D(":i2ov3", "IBP2_Out_V3");
        BIND_D(":i2oa1", "IBP2_Out_A1"); BIND_D(":i2oa2", "IBP2_Out_A2"); BIND_D(":i2oa3", "IBP2_Out_A3");
        BIND_D(":i2ow1", "IBP2_Out_W1"); BIND_D(":i2ow2", "IBP2_Out_W2"); BIND_D(":i2ow3", "IBP2_Out_W3");
        BIND_D(":i2bv", "IBP2_Bat_V"); BIND_D(":i2ba", "IBP2_Bat_A"); BIND_D(":i2bz", "IBP2_Bat_Zarad");

        BIND_D(":i3ih", "IBP3_In_H"); BIND_D(":i3iv1", "IBP3_In_V1"); BIND_D(":i3iv2", "IBP3_In_V2"); BIND_D(":i3iv3", "IBP3_In_V3");
        BIND_D(":i3ia1", "IBP3_In_A1"); BIND_D(":i3ia2", "IBP3_In_A2"); BIND_D(":i3ia3", "IBP3_In_A3");
        BIND_D(":i3iw1", "IBP3_In_W1"); BIND_D(":i3iw2", "IBP3_In_W2"); BIND_D(":i3iw3", "IBP3_In_W3");
        BIND_D(":i3bh", "IBP3_Bypass_H"); BIND_D(":i3bv1", "IBP3_Bypass_V1"); BIND_D(":i3bv2", "IBP3_Bypass_V2"); BIND_D(":i3bv3", "IBP3_Bypass_V3");
        BIND_D(":i3oh", "IBP3_Out_H"); BIND_D(":i3ov1", "IBP3_Out_V1"); BIND_D(":i3ov2", "IBP3_Out_V2"); BIND_D(":i3ov3", "IBP3_Out_V3");
        BIND_D(":i3oa1", "IBP3_Out_A1"); BIND_D(":i3oa2", "IBP3_Out_A2"); BIND_D(":i3oa3", "IBP3_Out_A3");
        BIND_D(":i3ow1", "IBP3_Out_W1"); BIND_D(":i3ow2", "IBP3_Out_W2"); BIND_D(":i3ow3", "IBP3_Out_W3");
        BIND_D(":i3bv", "IBP3_Bat_V"); BIND_D(":i3ba", "IBP3_Bat_A"); BIND_D(":i3bz", "IBP3_Bat_Zarad");

        BIND_D(":t1", "Temperatura1");
        BIND_D(":t2", "Temperatura2");
        BIND_S(":err", "ErrorsAll");
        BIND_S(":p1", "ProtokolIBP1");
        BIND_S(":p2", "ProtokolIBP2");
        BIND_S(":p3", "ProtokolIBP3");

        #undef BIND_D
        #undef BIND_S

        break;
    }

    default:
        qWarning() << "[DB Log] Неизвестный тип документа:" << docType;
        return false;
    }

    if (!query.exec()) {
        qWarning() << "[DB Log] Ошибка записи в таблицу" << safeTableName << ":" << query.lastError().text();
        return false;
    }

    return true;
}
