#include "snmpclient.h"
#include <QUdpSocket>
#include <QHostAddress>
#include <QRegularExpression>  // ✅ Нужен для parseSnmpResponse

// ============================================================================
// ✅ КОНСТРУКТОР / ДЕСТРУКТОР
// ============================================================================

SnmpClient::SnmpClient(QObject *parent)
    : QObject(parent)
    , m_requestId(1)  // ✅ Инициализируем счётчик запросов
{
    // ✅ Сокет больше не создаётся здесь — он создаётся локально в sendAndWait()
}

SnmpClient::~SnmpClient()
{
    qDebug() << "[SnmpClient] Деструктор вызван";
    // ✅ Ничего очищать не нужно, сокет создается локально
    qDebug() << "[SnmpClient] Деструктор завершён";
}

// ============================================================================
// ✅ SNMP GET запрос
// ============================================================================
bool SnmpClient::get(const QString &ip, int port, const QString &community,
                     const QString &oid, QVariant &result, int timeoutMs)
{
    QByteArray packet = buildSnmpPacket(SNMP_PDU_GET, m_requestId++,
                                        community, oid);
    QByteArray response;
    if (!sendAndWait(ip, port, packet, response, timeoutMs)) {
        m_lastError = "Ошибка отправки/приёма SNMP GET запроса";
        qWarning() << "[SnmpClient] GET:" << m_lastError << "для OID:" << oid;
        return false;
    }

    QString resultOid;
    int errorStatus = 0;
    if (!parseSnmpResponse(response, resultOid, result, errorStatus)) {
        m_lastError = "Ошибка парсинга SNMP ответа";
        qWarning() << "[SnmpClient] GET:" << m_lastError;
        return false;
    }

    if (errorStatus != 0) {
        m_lastError = QString("SNMP errorStatus = %1 (OID не найден или недоступен)").arg(errorStatus);
        qWarning() << "[SnmpClient] GET:" << m_lastError << "для OID:" << oid;
        return false;
    }

    m_lastError.clear();
    return true;
}

// ============================================================================
// ✅ SNMP GETNEXT запрос
// ============================================================================
bool SnmpClient::getNext(const QString &ip, int port, const QString &community,
                         const QString &oid, QString &resultOid, QVariant &result, int timeoutMs)
{
    QByteArray packet = buildSnmpPacket(SNMP_PDU_GETNEXT, m_requestId++,
                                        community, oid);
    QByteArray response;
    if (!sendAndWait(ip, port, packet, response, timeoutMs)) {
        m_lastError = "Ошибка отправки/приёма SNMP GETNEXT запроса";
        qWarning() << "[SnmpClient] GETNEXT:" << m_lastError << "для OID:" << oid;
        return false;
    }

    int errorStatus = 0;
    if (!parseSnmpResponse(response, resultOid, result, errorStatus)) {
        m_lastError = "Ошибка парсинга SNMP ответа";
        qWarning() << "[SnmpClient] GETNEXT:" << m_lastError;
        return false;
    }

    if (errorStatus != 0) {
        m_lastError = QString("SNMP errorStatus = %1").arg(errorStatus);
        qWarning() << "[SnmpClient] GETNEXT:" << m_lastError << "для OID:" << oid;
        return false;
    }

    m_lastError.clear();
    return true;
}

// ============================================================================
// ✅ SNMP SET запрос (для команд управления)
// ============================================================================
bool SnmpClient::set(const QString &ip, int port, const QString &community,
                     const QString &oid, const QVariant &value, int asnType, int timeoutMs)
{
    QByteArray packet = buildSnmpSetPacket(m_requestId++, community, oid, value, asnType);
    QByteArray response;
    if (!sendAndWait(ip, port, packet, response, timeoutMs)) {
        m_lastError = "Ошибка отправки/приёма SNMP SET запроса";
        qWarning() << "[SnmpClient] SET:" << m_lastError << "для OID:" << oid;
        return false;
    }

    QString resultOid;
    QVariant result;
    int errorStatus = 0;
    if (!parseSnmpResponse(response, resultOid, result, errorStatus)) {
        m_lastError = "Ошибка парсинга SNMP ответа";
        qWarning() << "[SnmpClient] SET:" << m_lastError;
        return false;
    }

    if (errorStatus != 0) {
        m_lastError = QString("SNMP errorStatus = %1 (запись запрещена или OID недоступен)").arg(errorStatus);
        qWarning() << "[SnmpClient] SET:" << m_lastError << "для OID:" << oid;
        return false;
    }

    m_lastError.clear();
    return true;
}

// ============================================================================
// ✅ SNMP WALK (обход дерева)
// ============================================================================
bool SnmpClient::walk(const QString &ip, int port, const QString &community,
                      const QString &baseOid, QMap<QString, QVariant> &results, int timeoutMs)
{
    QString currentOid = baseOid;
    int maxIterations = 1000; // Защита от бесконечного цикла

    while (maxIterations-- > 0) {
        QString resultOid;
        QVariant result;
        if (!getNext(ip, port, community, currentOid, resultOid, result, timeoutMs)) {
            break;
        }

        // Проверяем, что результат всё ещё под базовым OID
        if (!resultOid.startsWith(baseOid)) {
            break;
        }

        results[resultOid] = result;
        currentOid = resultOid;
    }

    return !results.isEmpty();
}

// ============================================================================
// ✅ КОДИРОВАНИЕ ДЛИНЫ (ASN.1/BER)
// ============================================================================
QByteArray SnmpClient::encodeLength(int length)
{
    QByteArray result;
    if (length < 128) {
        result.append(static_cast<char>(length));
    } else if (length < 256) {
        result.append(static_cast<char>(0x81));
        result.append(static_cast<char>(length));
    } else {
        result.append(static_cast<char>(0x82));
        result.append(static_cast<char>((length >> 8) & 0xFF));
        result.append(static_cast<char>(length & 0xFF));
    }
    return result;
}

// ============================================================================
// ✅ КОДИРОВАНИЕ INTEGER
// ============================================================================
QByteArray SnmpClient::encodeInteger(qint32 value)
{
    QByteArray result;
    result.append(static_cast<char>(ASN_INTEGER));

    QByteArray temp;
    if (value == 0) {
        temp.append(static_cast<char>(0));
    } else {
        qint32 v = value;
        while (v != 0 && v != -1) {
            temp.prepend(static_cast<char>(v & 0xFF));
            v >>= 8;
        }
        if (!temp.isEmpty()) {
            if ((temp[0] & 0x80) && value >= 0) {
                temp.prepend(static_cast<char>(0x00));
            } else if (!(temp[0] & 0x80) && value < 0) {
                temp.prepend(static_cast<char>(0xFF));
            }
        }
    }

    result.append(encodeLength(temp.size()));
    result.append(temp);
    return result;
}

// ============================================================================
// ✅ КОДИРОВАНИЕ OCTET STRING
// ============================================================================
QByteArray SnmpClient::encodeOctetString(const QByteArray &data)
{
    QByteArray result;
    result.append(static_cast<char>(ASN_OCTET_STRING));
    result.append(encodeLength(data.size()));
    result.append(data);
    return result;
}

// ============================================================================
// ✅ КОДИРОВАНИЕ NULL
// ============================================================================
QByteArray SnmpClient::encodeNull()
{
    QByteArray result;
    result.append(static_cast<char>(ASN_NULL));
    result.append(static_cast<char>(0));
    return result;
}

// ============================================================================
// ✅ КОДИРОВАНИЕ OID
// ============================================================================
QByteArray SnmpClient::encodeOid(const QString &oid)
{
    QStringList parts = oid.split('.', Qt::SkipEmptyParts);
    if (parts.size() < 2) return QByteArray();

    QByteArray encoded;

    // Первые два компонента кодируются как first*40 + second
    int first = parts[0].toInt();
    int second = parts[1].toInt();
    encoded.append(static_cast<char>(first * 40 + second));

    // Остальные компоненты — base-128 кодирование
    for (int i = 2; i < parts.size(); ++i) {
        int value = parts[i].toInt();
        if (value < 128) {
            encoded.append(static_cast<char>(value));
        } else {
            QByteArray temp;
            temp.prepend(static_cast<char>(value & 0x7F));
            value >>= 7;
            while (value > 0) {
                temp.prepend(static_cast<char>((value & 0x7F) | 0x80));
                value >>= 7;
            }
            encoded.append(temp);
        }
    }

    QByteArray result;
    result.append(static_cast<char>(ASN_OBJECT_ID));
    result.append(encodeLength(encoded.size()));
    result.append(encoded);
    return result;
}

// ============================================================================
// ✅ КОДИРОВАНИЕ SEQUENCE
// ============================================================================
QByteArray SnmpClient::encodeSequence(const QByteArray &content)
{
    QByteArray result;
    result.append(static_cast<char>(ASN_SEQUENCE));
    result.append(encodeLength(content.size()));
    result.append(content);
    return result;
}

QByteArray SnmpClient::encodeSequenceOf(const QByteArray &content)
{
    return encodeSequence(content);
}

// ============================================================================
// ✅ СБОРКА SNMP ПАКЕТА (GET/GETNEXT)
// ============================================================================
QByteArray SnmpClient::buildSnmpPacket(quint8 pduType, int requestId,
                                       const QString &community,
                                       const QString &oid)
{
    QByteArray version = encodeInteger(0);  // SNMPv1
    QByteArray comm = encodeOctetString(community.toUtf8());
    QByteArray reqId = encodeInteger(requestId);
    QByteArray errorStatus = encodeInteger(0);
    QByteArray errorIndex = encodeInteger(0);

    QByteArray varBindOid = encodeOid(oid);
    QByteArray varBindValue = encodeNull();
    QByteArray varBind = encodeSequence(varBindOid + varBindValue);
    QByteArray varBindList = encodeSequenceOf(varBind);

    QByteArray pduContent = reqId + errorStatus + errorIndex + varBindList;
    QByteArray pdu;
    pdu.append(static_cast<char>(pduType));
    pdu.append(encodeLength(pduContent.size()));
    pdu.append(pduContent);

    return encodeSequence(version + comm + pdu);
}

// ============================================================================
// ✅ СБОРКА SNMP SET ПАКЕТА
// ============================================================================
QByteArray SnmpClient::buildSnmpSetPacket(int requestId, const QString &community,
                                          const QString &oid, const QVariant &value, int asnType)
{
    QByteArray version = encodeInteger(0);
    QByteArray comm = encodeOctetString(community.toUtf8());
    QByteArray reqId = encodeInteger(requestId);
    QByteArray errorStatus = encodeInteger(0);
    QByteArray errorIndex = encodeInteger(0);

    QByteArray varBindOid = encodeOid(oid);
    QByteArray varBindValue;

    if (asnType == ASN_INTEGER) {
        varBindValue = encodeInteger(value.toInt());
    } else if (asnType == ASN_OCTET_STRING) {
        varBindValue = encodeOctetString(value.toString().toUtf8());
    } else {
        varBindValue = encodeNull();
    }

    QByteArray varBind = encodeSequence(varBindOid + varBindValue);
    QByteArray varBindList = encodeSequenceOf(varBind);

    QByteArray pduContent = reqId + errorStatus + errorIndex + varBindList;
    QByteArray pdu;
    pdu.append(static_cast<char>(SNMP_PDU_SET));
    pdu.append(encodeLength(pduContent.size()));
    pdu.append(pduContent);

    return encodeSequence(version + comm + pdu);
}

// ============================================================================
// ✅ ОТПРАВКА И ОЖИДАНИЕ ОТВЕТА (ПОТОКОБЕЗОПАСНАЯ ВЕРСИЯ)
// ============================================================================
bool SnmpClient::sendAndWait(const QString &ip, int port, const QByteArray &packet,
                             QByteArray &response, int timeoutMs)
{
    QMutexLocker locker(&m_mutex);

    // ✅ КРИТИЧЕСКИ ВАЖНО: Создаём сокет ЛОКАЛЬНО на стеке!
    // Это гарантирует, что сокет принадлежит текущему рабочему потоку (PollChain),
    // а не главному потоку, что полностью устраняет предупреждение Qt.
    QUdpSocket socket;

    // Привязываем сокет к любому свободному порту
    if (!socket.bind()) {
        m_lastError = "Не удалось привязать сокет";
        qWarning() << "[SnmpClient]" << m_lastError;
        return false;
    }

    // Отправляем пакет
    qint64 bytesSent = socket.writeDatagram(packet, QHostAddress(ip), quint16(port));
    if (bytesSent != packet.size()) {
        m_lastError = "Ошибка отправки пакета";
        qWarning() << "[SnmpClient]" << m_lastError;
        return false;
    }

    // Ожидаем ответ с таймаутом
    if (!socket.waitForReadyRead(timeoutMs)) {
        m_lastError = "Таймаут ожидания ответа от " + ip;
        qWarning() << "[SnmpClient]" << m_lastError;
        return false;
    }

    // Читаем ответ
    if (socket.hasPendingDatagrams()) {
        response.resize(socket.pendingDatagramSize());
        socket.readDatagram(response.data(), response.size());
        m_lastError.clear();  // Успех, очищаем ошибку
        return true;
    }

    m_lastError = "Нет доступных дейтаграмм после таймаута";
    qWarning() << "[SnmpClient]" << m_lastError;
    return false;
}

// ============================================================================
// ✅ ДЕКОДИРОВАНИЕ ДЛИНЫ
// ============================================================================
int SnmpClient::decodeLength(const QByteArray &data, int &offset)
{
    if (offset >= data.size()) return 0;

    quint8 firstByte = static_cast<quint8>(data[offset++]);
    if (firstByte < 128) {
        return firstByte;
    } else if (firstByte == 0x81) {
        if (offset >= data.size()) return 0;
        return static_cast<quint8>(data[offset++]);
    } else if (firstByte == 0x82) {
        if (offset + 1 >= data.size()) return 0;
        int length = (static_cast<quint8>(data[offset]) << 8) |
                     static_cast<quint8>(data[offset + 1]);
        offset += 2;
        return length;
    }
    return 0;
}

// ============================================================================
// ✅ ДЕКОДИРОВАНИЕ INTEGER
// ============================================================================
qint32 SnmpClient::decodeInteger(const QByteArray &data, int &offset, int length)
{
    qint32 value = 0;
    for (int i = 0; i < length; ++i) {
        value = (value << 8) | static_cast<quint8>(data[offset++]);
    }

    // Знаковое расширение
    if (length > 0 && (data[offset - length] & 0x80)) {
        qint32 mask = ~((1 << (length * 8)) - 1);
        value |= mask;
    }

    return value;
}

// ============================================================================
// ✅ ДЕКОДИРОВАНИЕ OCTET STRING
// ============================================================================
QByteArray SnmpClient::decodeOctetString(const QByteArray &data, int &offset, int length)
{
    QByteArray result = data.mid(offset, length);
    offset += length;
    return result;
}

// ============================================================================
// ✅ ДЕКОДИРОВАНИЕ OID
// ============================================================================
QString SnmpClient::decodeOid(const QByteArray &data, int &offset, int length)
{
    if (length <= 0) return QString();

    QStringList parts;
    int endOffset = offset + length;

    // Первый байт: first*40 + second
    quint8 firstByte = static_cast<quint8>(data[offset++]);
    parts << QString::number(firstByte / 40);
    parts << QString::number(firstByte % 40);

    // Остальные компоненты
    while (offset < endOffset) {
        int value = 0;
        bool moreBytes = true;
        while (moreBytes && offset < endOffset) {
            quint8 byte = static_cast<quint8>(data[offset++]);
            value = (value << 7) | (byte & 0x7F);
            moreBytes = (byte & 0x80) != 0;
        }
        parts << QString::number(value);
    }

    return parts.join(".");
}

// ============================================================================
// ✅ ПАРСИНГ SNMP ОТВЕТА
// ============================================================================
bool SnmpClient::parseSnmpResponse(const QByteArray &response,
                                   QString &resultOid,
                                   QVariant &result,
                                   int &errorStatus)
{
    if (response.size() < 10) {
        qWarning() << "[SnmpClient] Ответ слишком короткий";
        return false;
    }

    int offset = 0;

    // 1. SEQUENCE (весь пакет)
    if (static_cast<quint8>(response[offset++]) != ASN_SEQUENCE) {
        qWarning() << "[SnmpClient] Ожидается SEQUENCE";
        return false;
    }
    int totalLen = decodeLength(response, offset);
    Q_UNUSED(totalLen);

    // 2. Version (INTEGER)
    if (static_cast<quint8>(response[offset++]) != ASN_INTEGER) {
        qWarning() << "[SnmpClient] Ожидается INTEGER (version)";
        return false;
    }
    int versionLen = decodeLength(response, offset);
    offset += versionLen;

    // 3. Community (OCTET STRING)
    if (static_cast<quint8>(response[offset++]) != ASN_OCTET_STRING) {
        qWarning() << "[SnmpClient] Ожидается OCTET STRING (community)";
        return false;
    }
    int commLen = decodeLength(response, offset);
    offset += commLen;

    // 4. PDU (RESPONSE = 0xA2)
    quint8 pduType = static_cast<quint8>(response[offset++]);
    if (pduType != SNMP_PDU_RESPONSE) {
        qWarning() << "[SnmpClient] Ожидается RESPONSE PDU, получено" << QString::number(pduType, 16);
        return false;
    }
    int pduLen = decodeLength(response, offset);
    Q_UNUSED(pduLen);

    // 5. Request ID (INTEGER)
    if (static_cast<quint8>(response[offset++]) != ASN_INTEGER) {
        qWarning() << "[SnmpClient] Ожидается INTEGER (request-id)";
        return false;
    }
    int reqIdLen = decodeLength(response, offset);
    offset += reqIdLen;

    // 6. Error Status (INTEGER)
    if (static_cast<quint8>(response[offset++]) != ASN_INTEGER) {
        qWarning() << "[SnmpClient] Ожидается INTEGER (error-status)";
        return false;
    }
    int errStatusLen = decodeLength(response, offset);
    errorStatus = decodeInteger(response, offset, errStatusLen);

    // 7. Error Index (INTEGER)
    if (static_cast<quint8>(response[offset++]) != ASN_INTEGER) {
        qWarning() << "[SnmpClient] Ожидается INTEGER (error-index)";
        return false;
    }
    int errIndexLen = decodeLength(response, offset);
    offset += errIndexLen;

    // 8. Variable Bindings (SEQUENCE OF)
    if (static_cast<quint8>(response[offset++]) != ASN_SEQUENCE) {
        qWarning() << "[SnmpClient] Ожидается SEQUENCE (var-bind-list)";
        return false;
    }
    int varBindListLen = decodeLength(response, offset);
    Q_UNUSED(varBindListLen);

    // 9. Variable Binding (SEQUENCE)
    if (static_cast<quint8>(response[offset++]) != ASN_SEQUENCE) {
        qWarning() << "[SnmpClient] Ожидается SEQUENCE (var-bind)";
        return false;
    }
    int varBindLen = decodeLength(response, offset);
    Q_UNUSED(varBindLen);

    // 10. OID
    if (static_cast<quint8>(response[offset++]) != ASN_OBJECT_ID) {
        qWarning() << "[SnmpClient] Ожидается OID";
        return false;
    }
    int oidLen = decodeLength(response, offset);
    resultOid = decodeOid(response, offset, oidLen);

    // 11. Value
    quint8 valueType = static_cast<quint8>(response[offset++]);
    int valueLen = decodeLength(response, offset);

    switch (valueType) {
        case ASN_INTEGER: {
            qint32 intValue = decodeInteger(response, offset, valueLen);
            result = intValue;
            break;
        }
        case ASN_OCTET_STRING: {
            QByteArray strData = decodeOctetString(response, offset, valueLen);
            QString strValue = QString::fromUtf8(strData);
            bool ok;
            double dVal = strValue.toDouble(&ok);
            QRegularExpression numRegex("^[0-9.\\-]+$");
            if (ok && numRegex.match(strValue).hasMatch()) {
                result = dVal;
            } else {
                result = strValue;
            }
            break;
        }
        case ASN_NULL: {
            result = QVariant();
            offset += valueLen;
            break;
        }
        case ASN_OBJECT_ID: {
            result = decodeOid(response, offset, valueLen);
            break;
        }
        default:
            qWarning() << "[SnmpClient] Неизвестный тип значения:" << QString::number(valueType, 16);
            result = QVariant();
            offset += valueLen;
            break;
    }

    return true;
}
