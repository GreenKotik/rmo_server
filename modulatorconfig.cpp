#include "modulatorconfig.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QRegularExpression>

// ============================================================================
// ✅ ЗАГРУЗКА КОНФИГА ДЛЯ deviceId
// ============================================================================
bool ModulatorConfig::loadForDevice(int deviceId, const QString &configDir)
{
    m_valid = false;
    m_model.clear();
    m_params.clear();
    m_alarmRules.clear();

    // 1. Определяем папку с конфигами
    QString dirPath = configDir;
    if (dirPath.isEmpty()) {
        dirPath = QCoreApplication::applicationDirPath() + "/drivers/modulators";
    }

    QDir dir(dirPath);
    if (!dir.exists()) {
        qWarning() << "[ModulatorConfig] Папка конфигов не найдена:" << dirPath;
        return false;
    }

    // 2. Перебираем все *.json, ищем первый, где deviceIds содержит deviceId
    const QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files);
    for (const QString &fileName : files) {
        const QString path = dir.filePath(fileName);
        if (parseFile(path, deviceId)) {
            m_valid = true;
            qDebug() << "[ModulatorConfig] Загружен конфиг:" << fileName
                     << "модель:" << m_model
                     << "параметров:" << m_params.size()
                     << "правил аварий:" << m_alarmRules.size()
                     << "для deviceId:" << deviceId;
            return true;
        }
    }

    qWarning() << "[ModulatorConfig] Конфиг для deviceId" << deviceId
               << "не найден в" << dirPath;
    return false;
}

// ============================================================================
// ✅ ПАРСИНГ ОДНОГО ФАЙЛА
// ============================================================================
bool ModulatorConfig::parseFile(const QString &path, int deviceId)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[ModulatorConfig] Не удалось открыть:" << path;
        return false;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[ModulatorConfig] Ошибка JSON в" << path
                   << ":" << err.errorString();
        return false;
    }
    if (!doc.isObject()) {
        qWarning() << "[ModulatorConfig] Корень не объект:" << path;
        return false;
    }

    const QJsonObject root = doc.object();

    // 1. Проверяем, что deviceId входит в deviceIds
    const QJsonArray deviceIds = root.value("deviceIds").toArray();
    bool found = false;
    for (const QJsonValue &v : deviceIds) {
        if (v.toInt() == deviceId) { found = true; break; }
    }
    if (!found) return false;   // не наш файл — это не ошибка

    // 2. Модель
    m_model = root.value("model").toString();

    // 3. Параметры
    m_params.clear();
    const QJsonArray paramsArr = root.value("params").toArray();
    for (const QJsonValue &v : paramsArr) {
        const QJsonObject p = v.toObject();
        ModulatorParam param;

        param.key    = p.value("key").toString();
        param.label = p.value("label").toString();
        param.oid    = p.value("oid").toString();
        param.type   = parseType(p.value("type").toString("double"));
        param.target = p.value("target").toString();
        param.scale  = p.value("scale").toDouble(1.0);
        param.oidTodo = p.value("oidTodo").toBool(false);

        param.writable      = p.value("writable").toBool(false);
        param.commandCode   = p.value("commandCode").toInt(0);
        param.min           = p.value("min").toDouble(0.0);
        param.max           = p.value("max").toDouble(0.0);
        param.snmpType      = p.value("snmpType").toInt(2);
        param.commandCodeTodo = p.value("commandCodeTodo").toBool(false);

        // Минимальная валидация
        if (param.key.isEmpty() || param.target.isEmpty()) {
            qWarning() << "[ModulatorConfig]" << path
                       << "параметр без key/target, пропущен";
            continue;
        }
        if (!isOidPlausible(param.oid)) {
            qWarning() << "[ModulatorConfig]" << path
                       << "подозрительный OID у" << param.key
                       << ":" << param.oid << "(оставляем, помечен oidTodo="
                       << param.oidTodo << ")";
        }
        if (param.writable && param.commandCode == 0) {
            qWarning() << "[ModulatorConfig]" << path
                       << "writable-параметр" << param.key
                       << "без commandCode — запись невозможна";
        }

        m_params.append(param);
    }

    // 4. Правила аварий
    m_alarmRules.clear();
    const QJsonArray rulesArr = root.value("alarmRules").toArray();
    for (const QJsonValue &v : rulesArr) {
        const QJsonObject r = v.toObject();
        AlarmRule rule;
        rule.field = r.value("field").toString();
        rule.op    = parseOp(r.value("op").toString("!="));
        rule.value = r.value("value").toDouble(0.0);
        rule.code  = r.value("code").toInt(0);

        if (rule.field.isEmpty() || rule.code == 0) {
            qWarning() << "[ModulatorConfig]" << path
                       << "правило аварии без field/code, пропущено";
            continue;
        }
        m_alarmRules.append(rule);
    }

    return true;
}

// ============================================================================
// ✅ ВСПОМОГАТЕЛЬНЫЕ
// ============================================================================
ParamType ModulatorConfig::parseType(const QString &s)
{
    const QString t = s.trimmed().toLower();
    if (t == "string") return ParamType::String;
    if (t == "int")    return ParamType::Int;
    if (t == "bool")   return ParamType::Bool;
    return ParamType::Double;
}

AlarmOp ModulatorConfig::parseOp(const QString &s)
{
    const QString t = s.trimmed();
    if (t == "==") return AlarmOp::Eq;
    if (t == "!=") return AlarmOp::Ne;
    if (t == ">")  return AlarmOp::Gt;
    if (t == "<")  return AlarmOp::Lt;
    if (t == ">=") return AlarmOp::Ge;
    if (t == "<=") return AlarmOp::Le;
    return AlarmOp::Ne;   // безопасный дефолт
}

bool ModulatorConfig::isOidPlausible(const QString &oid)
{
    // Простая эвристика: OID должен начинаться с точки, содержать только цифры и точки,
    // и иметь хотя бы 5 компонентов. Заглушки вида ".1.3.6.1.4.1.34186." (с точкой на конце,
    // но без последнего числа) — не проходят проверку и логируются как подозрительные.
    static const QRegularExpression re("^\\.[0-9]+(\\.[0-9]+){3,}$");
    return re.match(oid).hasMatch();
}

// ============================================================================
// ✅ ПОИСК ПАРАМЕТРОВ
// ============================================================================
const ModulatorParam *ModulatorConfig::findByCommandCode(int commandCode) const
{
    if (commandCode == 0) return nullptr;
    for (const ModulatorParam &p : m_params) {
        if (p.writable && p.commandCode == commandCode) return &p;
    }
    return nullptr;
}

const ModulatorParam *ModulatorConfig::findByTarget(const QString &target) const
{
    for (const ModulatorParam &p : m_params) {
        if (p.target == target) return &p;
    }
    return nullptr;
}
