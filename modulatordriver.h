#ifndef MODULATORDRIVER_H
#define MODULATORDRIVER_H

#include "idevicedriver.h"
#include "devicedata.h"
#include "snmpclient.h"
#include "modulatorconfig.h"
#include <QHash>
#include <functional>

// ============================================================================
// ✅ УНИВЕРСАЛЬНЫЙ SNMP-ДРАЙВЕР МОДУЛЯТОРА
//
// Модель модулятора (набор OID, типы, коэффициенты, правила аварий)
// задаётся JSON-конфигом в applicationDirPath()/drivers/modulators/*.json.
// Драйвер при создании ищет файл, в "deviceIds" которого есть его deviceId.
//
// Никаких OID/коэффициентов/кодов аварий в коде драйвера — всё в JSON.
// Таблица сеттеров/геттеров покрывает ВСЕ поля ModulatorData: JSON конкретной
// модели ссылается только на нужные ему target, остальные остаются дефолтными.
// ============================================================================
class ModulatorDriver : public IDeviceDriver
{
    Q_OBJECT
public:
    explicit ModulatorDriver(int deviceId, QObject *parent = nullptr);
    ~ModulatorDriver() override;

    bool connect() override;
    void disconnect() override;
    void setConnectionParams(const EquipmentConnection &conn) override;
    void setDeviceSettings(const QJsonObject &settings) override;  // игнорируется (конфиг из JSON)
    std::shared_ptr<IDeviceData> poll() override;
    bool executeCommand(const ControlCommand &cmd) override;
    // Список target-полей, поддерживаемых таблицей сеттеров/геттеров.
    // Используется редактором конфигов для валидации.
    static QStringList knownTargets();
    // Значение поля ModulatorData по target, как QVariant.
    // String-поля возвращают QString, остальные — double.
    // Пустой QVariant, если target неизвестен.
    static QVariant valueByTarget(const ModulatorData &data, const QString &target);

private:
    // Сеттер: (куда писать, значение из SNMP, scale из конфига)
    using Setter = std::function<void(ModulatorData&, const QVariant&, double)>;
    // Геттер: (откуда читать) → double (для alarmRules; строки не поддерживаются)
    using Getter = std::function<double(const ModulatorData&)>;

    void buildSetters();
    void buildGetters();

    bool readParam(const ModulatorParam &p, ModulatorData &data);
    void applyAlarmRules(ModulatorData &data);

    static bool evalAlarmRule(const AlarmRule &rule, double actual);

    SnmpClient      m_snmp;
    QString         m_ip;
    int             m_port = 161;
    QString         m_community = "public";
    ModulatorConfig m_config;

    QHash<QString, Setter> m_setters;
    QHash<QString, Getter> m_getters;
};

#endif // MODULATORDRIVER_H
