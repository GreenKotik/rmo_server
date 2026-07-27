#ifndef SNMPCLIENT_H
#define SNMPCLIENT_H

#include <QObject>
#include <QVariant>
#include <QMap>
#include <QString>
#include <QByteArray>
#include <QDebug>
#include <QMutex>

// ============================================================================
// ✅ SNMP-КЛИЕНТ (чистый Qt, без внешних библиотек)
// Поддержка: SNMP v1, v2c
// Команды: GET, GETNEXT, SET, WALK
//
// ВАЖНО: QUdpSocket создаётся ЛОКАЛЬНО в методе sendAndWait(),
// что гарантирует потокобезопасность (сокет принадлежит рабочему потоку).
// ============================================================================
class SnmpClient : public QObject
{
    Q_OBJECT
public:
    explicit SnmpClient(QObject *parent = nullptr);
    ~SnmpClient();

    // ✅ SNMP GET: Получить значение по OID
    bool get(const QString &ip, int port, const QString &community,
             const QString &oid, QVariant &result, int timeoutMs = 2000);

    // ✅ SNMP GETNEXT: Получить следующее значение в дереве
    bool getNext(const QString &ip, int port, const QString &community,
                 const QString &oid, QString &resultOid, QVariant &result, int timeoutMs = 2000);

    // ✅ SNMP SET: Установить значение (для команд управления)
    // asnType: ASN_INTEGER (0x02) или ASN_OCTET_STRING (0x04)
    bool set(const QString &ip, int port, const QString &community,
             const QString &oid, const QVariant &value, int asnType, int timeoutMs = 2000);

    // ✅ SNMP WALK: Обойти всё поддерево OID
    bool walk(const QString &ip, int port, const QString &community,
              const QString &baseOid, QMap<QString, QVariant> &results, int timeoutMs = 2000);

    // ✅ Получить последнюю ошибку
    QString lastError() const { return m_lastError; }

private:
    // ✅ ASN.1/BER константы
    static constexpr quint8 ASN_INTEGER       = 0x02;
    static constexpr quint8 ASN_OCTET_STRING  = 0x04;
    static constexpr quint8 ASN_NULL          = 0x05;
    static constexpr quint8 ASN_OBJECT_ID     = 0x06;
    static constexpr quint8 ASN_SEQUENCE      = 0x30;

    // ✅ SNMP PDU типы
    static constexpr quint8 SNMP_PDU_GET      = 0xA0;
    static constexpr quint8 SNMP_PDU_GETNEXT  = 0xA1;
    static constexpr quint8 SNMP_PDU_RESPONSE = 0xA2;
    static constexpr quint8 SNMP_PDU_SET      = 0xA3;

    // ✅ Кодирование ASN.1/BER
    QByteArray encodeLength(int length);
    QByteArray encodeInteger(qint32 value);
    QByteArray encodeOctetString(const QByteArray &data);
    QByteArray encodeNull();
    QByteArray encodeOid(const QString &oid);
    QByteArray encodeSequence(const QByteArray &content);
    QByteArray encodeSequenceOf(const QByteArray &content);

    // ✅ Сборка SNMP пакетов
    QByteArray buildSnmpPacket(quint8 pduType, int requestId,
                               const QString &community,
                               const QString &oid);
    QByteArray buildSnmpSetPacket(int requestId, const QString &community,
                                  const QString &oid, const QVariant &value, int asnType);

    // ✅ Парсинг SNMP ответа
    bool parseSnmpResponse(const QByteArray &response,
                           QString &resultOid,
                           QVariant &result,
                           int &errorStatus);

    // ✅ Декодирование ASN.1/BER
    int decodeLength(const QByteArray &data, int &offset);
    qint32 decodeInteger(const QByteArray &data, int &offset, int length);
    QByteArray decodeOctetString(const QByteArray &data, int &offset, int length);
    QString decodeOid(const QByteArray &data, int &offset, int length);

    // ✅ Вспомогательные методы
    bool sendAndWait(const QString &ip, int port, const QByteArray &packet,
                     QByteArray &response, int timeoutMs);

    // ✅ ПОТОКОБЕЗОПАСНЫЕ поля (защищены m_mutex)
    int m_requestId;
    QString m_lastError;
    QMutex m_mutex;
};

#endif // SNMPCLIENT_H
