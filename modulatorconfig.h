#ifndef MODULATORCONFIG_H
#define MODULATORCONFIG_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

// ============================================================================
// ✅ УНИВЕРСАЛЬНЫЙ КОНФИГ МОДЕЛИ МОДУЛЯТОРА
//
// Один JSON-файл = одна модель (или группа моделей с одинаковым набором OID).
// Файлы лежат в: applicationDirPath()/drivers/modulators/*.json
//
// Драйвер ModulatorDriver при создании ищет файл, в "deviceIds" которого
// есть его deviceId, и грузит параметры/правила аварий оттуда.
// Никаких OID/коэффициентов/кодов аварий в коде драйвера — всё в JSON.
// ============================================================================

// Тип значения параметра — как парсить ответ SNMP и что класть в ModulatorData
enum class ParamType {
    String,   // OCTET STRING → QString
    Int,      // INTEGER → int
    Double,   // OCTET STRING с числом / INTEGER → double
    Bool      // INTEGER 0/1 → int (в ModulatorData хранится как int)
};

// Описание одного параметра модулятора
struct ModulatorParam {
    QString   key;              // уникальный ключ (для логов и params команды)
    QString   label;            // Название параметра на русском
    QString   oid;              // OID для SNMP GET
    ParamType type = ParamType::Double;
    QString   target;           // имя поля ModulatorData (для таблицы сеттеров)
    double    scale = 1.0;      // множитель (например, 0.1 для outputRfLbBandPowerLevel)
    bool      oidTodo = false;  // true, если OID — заглушка (помечено в JSON)

    // Для команд (SNMP SET)
    bool      writable = false;
    int       commandCode = 0;  // код команды из AlarmRegistry (1610+)
    double    min = 0.0;
    double    max = 0.0;
    int       snmpType = 2;     // ASN.1 тип для SET: 2=INTEGER, 4=OCTET STRING
    bool      commandCodeTodo = false; // true, если commandCode — заглушка
};

// Оператор сравнения в правиле аварии
enum class AlarmOp {
    Eq,   // ==
    Ne,   // !=
    Gt,   // >
    Lt,   // <
    Ge,   // >=
    Le    // <=
};

// Правило аварии: если <field> <op> <value> — добавить код <code> в errors
// Пример JSON: { "field": "monitorRelayWarnInState", "op": "!=", "value": 0, "code": 1602 }
struct AlarmRule {
    QString field;       // имя поля ModulatorData (совпадает с target)
    AlarmOp op = AlarmOp::Ne;
    double  value = 0.0;
    int     code = 0;    // код аварии из AlarmRegistry
};

// ============================================================================
// ✅ ЗАГРУЗЧИК КОНФИГА
// ============================================================================
class ModulatorConfig
{
public:
    ModulatorConfig() = default;

    // Найти и загрузить конфиг для deviceId.
    // Поиск: applicationDirPath()/drivers/modulators/*.json
    // Возвращает false и пишет qWarning при ошибке (файл не найден, битый JSON,
    // deviceId не найден ни в одном файле).
    bool loadForDevice(int deviceId, const QString &configDir = QString());

    // Готов ли конфиг к использованию (успешно загружен)
    bool isValid() const { return m_valid; }

    QString model() const { return m_model; }
    const QVector<ModulatorParam> &params() const { return m_params; }
    const QVector<AlarmRule> &alarmRules() const { return m_alarmRules; }

    // Найти параметр по commandCode (для executeCommand)
    const ModulatorParam *findByCommandCode(int commandCode) const;

    // Найти параметр по target (для таблицы сеттеров/диагностики)
    const ModulatorParam *findByTarget(const QString &target) const;

private:
    bool parseFile(const QString &path, int deviceId);
    static ParamType parseType(const QString &s);
    static AlarmOp  parseOp(const QString &s);
    static bool     isOidPlausible(const QString &oid);

    bool                       m_valid = false;
    QString                    m_model;
    QVector<ModulatorParam>    m_params;
    QVector<AlarmRule>         m_alarmRules;
};

#endif // MODULATORCONFIG_H
