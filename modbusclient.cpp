#include "modbusclient.h"
#include <QEventLoop>
#include <QTimer>
#include <cstring>
#include <QElapsedTimer>

// ============================================================================
// ✅ КОДЫ ФУНКЦИЙ MODBUS
// ============================================================================
#define MODBUS_FUNC_READ_COILS              0x01
#define MODBUS_FUNC_READ_DISCRETE_INPUTS    0x02
#define MODBUS_FUNC_READ_HOLDING_REGISTERS  0x03
#define MODBUS_FUNC_READ_INPUT_REGISTERS    0x04
#define MODBUS_FUNC_WRITE_SINGLE_COIL       0x05
#define MODBUS_FUNC_WRITE_SINGLE_REGISTER   0x06
#define MODBUS_FUNC_WRITE_MULTIPLE_COILS    0x0F
#define MODBUS_FUNC_WRITE_MULTIPLE_REGS     0x10

// ============================================================================
// ✅ КОДЫ ИСКЛЮЧЕНИЙ MODBUS
// ============================================================================
#define MODBUS_EXC_ILLEGAL_FUNCTION         0x01
#define MODBUS_EXC_ILLEGAL_DATA_ADDRESS     0x02
#define MODBUS_EXC_ILLEGAL_DATA_VALUE       0x03
#define MODBUS_EXC_SLAVE_DEVICE_FAILURE     0x04

// ============================================================================
// ✅ КОНСТРУКТОР / ДЕСТРУКТОР
// ============================================================================

ModbusClient::ModbusClient(QObject *parent)
    : QObject(parent)
    , m_mode(Mode::TCP)
    , m_tcpSocket(new QTcpSocket(this))
    , m_serialPort(new QSerialPort(this))
    , m_transactionId(0)
{
}

ModbusClient::~ModbusClient()
{
    disconnect();
}

// ============================================================================
//  ПОДКЛЮЧЕНИЕ TCP
// ============================================================================

bool ModbusClient::connectTcp(const QString &ip, quint16 port, int timeoutMs)
{
    QMutexLocker locker(&m_mutex);

    disconnect();
    m_mode = Mode::TCP;

    m_tcpSocket->connectToHost(ip, port);

    if (!m_tcpSocket->waitForConnected(timeoutMs)) {
        m_lastError = "Не удалось подключиться к " + ip + ":" + QString::number(port);
        qWarning() << "[ModbusClient]" << m_lastError;
        return false;
    }

    qDebug() << "[ModbusClient] Подключено к" << ip << ":" << port;
    emit connected();
    return true;
}

// ============================================================================
//  ПОДКЛЮЧЕНИЕ RTU (COM-ПОРТ)
// ============================================================================

bool ModbusClient::connectRtu(const QString &portName, int baudRate,
                              QSerialPort::DataBits dataBits,
                              QSerialPort::Parity parity,
                              QSerialPort::StopBits stopBits,
                              int timeoutMs)
{
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(timeoutMs);

    disconnect();
    m_mode = Mode::RTU;

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(dataBits);
    m_serialPort->setParity(parity);
    m_serialPort->setStopBits(stopBits);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        m_lastError = "Не удалось открыть COM-порт " + portName;
        qWarning() << "[ModbusClient]" << m_lastError;
        return false;
    }

    qDebug() << "[ModbusClient] Открыт COM-порт" << portName << "скорость" << baudRate;
    emit connected();
    return true;
}

// ============================================================================
//  ОТКЛЮЧЕНИЕ
// ============================================================================

void ModbusClient::disconnect()
{
    QMutexLocker locker(&m_mutex);

    if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
        m_tcpSocket->disconnectFromHost();
        m_tcpSocket->waitForDisconnected(500);
    }

    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }

    emit disconnected();
}

bool ModbusClient::isConnected() const
{
    return (m_tcpSocket->state() == QAbstractSocket::ConnectedState)
        || m_serialPort->isOpen();
}

// ============================================================================
//  ФУНКЦИЯ 0x01: READ COILS
// ============================================================================

bool ModbusClient::readCoils(quint8 slaveId, quint16 startAddr, quint16 quantity,
                             QVector<bool> &values, int timeoutMs)
{
    if (!isConnected()) {
        m_lastError = "Не подключено";
        return false;
    }

    // Формируем PDU: FC + StartAddr + Quantity
    QByteArray pdu;
    pdu.append(static_cast<char>(MODBUS_FUNC_READ_COILS));
    pdu.append(static_cast<char>((startAddr >> 8) & 0xFF));
    pdu.append(static_cast<char>(startAddr & 0xFF));
    pdu.append(static_cast<char>((quantity >> 8) & 0xFF));
    pdu.append(static_cast<char>(quantity & 0xFF));

    // Формируем запрос
    QByteArray request;
    if (m_mode == Mode::TCP) {
        request = createMBAPHeader(++m_transactionId, pdu.size() + 1, slaveId);
        request.append(pdu);
    } else if (m_mode == Mode::RTU) {
        request = createRTUFrame(slaveId, pdu);
    } else {
        request = createASCIIFrame(slaveId, pdu);
    }

    // Отправляем и получаем ответ
    QByteArray response;
    if (!sendRequest(request, response, timeoutMs)) {
        return false;
    }

    // Парсим ответ
    QByteArray data;
    int exceptionCode = 0;
    if (!parseResponse(response, MODBUS_FUNC_READ_COILS, data, exceptionCode)) {
        return false;
    }

    if (exceptionCode != 0) {
        m_lastError = QString("Исключение Modbus: код %1").arg(exceptionCode);
        return false;
    }

    // Извлекаем биты
    values.clear();
    for (int i = 0; i < quantity; ++i) {
        int byteIndex = i / 8;
        int bitIndex = i % 8;
        bool value = (static_cast<quint8>(data[byteIndex]) >> bitIndex) & 0x01;
        values.append(value);
    }

    return true;
}

// ============================================================================
//  ФУНКЦИЯ 0x02: READ DISCRETE INPUTS
// ============================================================================

bool ModbusClient::readDiscreteInputs(quint8 slaveId, quint16 startAddr, quint16 quantity,
                                      QVector<bool> &values, int timeoutMs)
{
    if (!isConnected()) {
        m_lastError = "Не подключено";
        return false;
    }

    QByteArray pdu;
    pdu.append(static_cast<char>(MODBUS_FUNC_READ_DISCRETE_INPUTS));
    pdu.append(static_cast<char>((startAddr >> 8) & 0xFF));
    pdu.append(static_cast<char>(startAddr & 0xFF));
    pdu.append(static_cast<char>((quantity >> 8) & 0xFF));
    pdu.append(static_cast<char>(quantity & 0xFF));

    QByteArray request;
    if (m_mode == Mode::TCP) {
        request = createMBAPHeader(++m_transactionId, pdu.size() + 1, slaveId);
        request.append(pdu);
    } else if (m_mode == Mode::RTU) {
        request = createRTUFrame(slaveId, pdu);
    } else {
        request = createASCIIFrame(slaveId, pdu);
    }

    QByteArray response;
    if (!sendRequest(request, response, timeoutMs)) {
        return false;
    }

    QByteArray data;
    int exceptionCode = 0;
    if (!parseResponse(response, MODBUS_FUNC_READ_DISCRETE_INPUTS, data, exceptionCode)) {
        return false;
    }

    if (exceptionCode != 0) {
        m_lastError = QString("Исключение Modbus: код %1").arg(exceptionCode);
        return false;
    }

    values.clear();
    for (int i = 0; i < quantity; ++i) {
        int byteIndex = i / 8;
        int bitIndex = i % 8;
        bool value = (static_cast<quint8>(data[byteIndex]) >> bitIndex) & 0x01;
        values.append(value);
    }

    return true;
}

// ============================================================================
//  ФУНКЦИЯ 0x03: READ HOLDING REGISTERS
// ============================================================================

bool ModbusClient::readHoldingRegisters(quint8 slaveId, quint16 startAddr, quint16 quantity,
                                        QVector<quint16> &values, int timeoutMs)
{
    if (!isConnected()) {
        m_lastError = "Не подключено";
        return false;
    }

    QByteArray pdu;
    pdu.append(static_cast<char>(MODBUS_FUNC_READ_HOLDING_REGISTERS));
    pdu.append(static_cast<char>((startAddr >> 8) & 0xFF));
    pdu.append(static_cast<char>(startAddr & 0xFF));
    pdu.append(static_cast<char>((quantity >> 8) & 0xFF));
    pdu.append(static_cast<char>(quantity & 0xFF));

    QByteArray request;
    if (m_mode == Mode::TCP) {
        request = createMBAPHeader(++m_transactionId, pdu.size() + 1, slaveId);
        request.append(pdu);
    } else if (m_mode == Mode::RTU) {
        request = createRTUFrame(slaveId, pdu);
    } else {
        request = createASCIIFrame(slaveId, pdu);
    }

    QByteArray response;
    if (!sendRequest(request, response, timeoutMs)) {
        return false;
    }

    QByteArray data;
    int exceptionCode = 0;
    if (!parseResponse(response, MODBUS_FUNC_READ_HOLDING_REGISTERS, data, exceptionCode)) {
        return false;
    }

    if (exceptionCode != 0) {
        m_lastError = QString("Исключение Modbus: код %1").arg(exceptionCode);
        return false;
    }

    // Извлекаем регистры (Big-Endian)
    values.clear();
    for (int i = 0; i < quantity; ++i) {
        quint16 value = (static_cast<quint8>(data[i * 2]) << 8) |
                        static_cast<quint8>(data[i * 2 + 1]);
        values.append(value);
    }

    return true;
}

// ============================================================================
//  ФУНКЦИЯ 0x04: READ INPUT REGISTERS
// ============================================================================

bool ModbusClient::readInputRegisters(quint8 slaveId, quint16 startAddr, quint16 quantity,
                                      QVector<quint16> &values, int timeoutMs)
{
    if (!isConnected()) {
        m_lastError = "Не подключено";
        return false;
    }

    QByteArray pdu;
    pdu.append(static_cast<char>(MODBUS_FUNC_READ_INPUT_REGISTERS));
    pdu.append(static_cast<char>((startAddr >> 8) & 0xFF));
    pdu.append(static_cast<char>(startAddr & 0xFF));
    pdu.append(static_cast<char>((quantity >> 8) & 0xFF));
    pdu.append(static_cast<char>(quantity & 0xFF));

    QByteArray request;
    if (m_mode == Mode::TCP) {
        request = createMBAPHeader(++m_transactionId, pdu.size() + 1, slaveId);
        request.append(pdu);
    } else if (m_mode == Mode::RTU) {
        request = createRTUFrame(slaveId, pdu);
    } else {
        request = createASCIIFrame(slaveId, pdu);
    }

    QByteArray response;
    if (!sendRequest(request, response, timeoutMs)) {
        return false;
    }

    QByteArray data;
    int exceptionCode = 0;
    if (!parseResponse(response, MODBUS_FUNC_READ_INPUT_REGISTERS, data, exceptionCode)) {
        return false;
    }

    if (exceptionCode != 0) {
        m_lastError = QString("Исключение Modbus: код %1").arg(exceptionCode);
        return false;
    }

    values.clear();
    for (int i = 0; i < quantity; ++i) {
        quint16 value = (static_cast<quint8>(data[i * 2]) << 8) |
                        static_cast<quint8>(data[i * 2 + 1]);
        values.append(value);
    }

    return true;
}

// ============================================================================
//  ФУНКЦИЯ 0x05: WRITE SINGLE COIL
// ============================================================================

bool ModbusClient::writeSingleCoil(quint8 slaveId, quint16 addr, bool value, int timeoutMs)
{
    if (!isConnected()) {
        m_lastError = "Не подключено";
        return false;
    }

    QByteArray pdu;
    pdu.append(static_cast<char>(MODBUS_FUNC_WRITE_SINGLE_COIL));
    pdu.append(static_cast<char>((addr >> 8) & 0xFF));
    pdu.append(static_cast<char>(addr & 0xFF));
    pdu.append(static_cast<char>(value ? 0xFF : 0x00));
    pdu.append(static_cast<char>(0x00));

    QByteArray request;
    if (m_mode == Mode::TCP) {
        request = createMBAPHeader(++m_transactionId, pdu.size() + 1, slaveId);
        request.append(pdu);
    } else if (m_mode == Mode::RTU) {
        request = createRTUFrame(slaveId, pdu);
    } else {
        request = createASCIIFrame(slaveId, pdu);
    }

    QByteArray response;
    if (!sendRequest(request, response, timeoutMs)) {
        return false;
    }

    QByteArray data;
    int exceptionCode = 0;
    if (!parseResponse(response, MODBUS_FUNC_WRITE_SINGLE_COIL, data, exceptionCode)) {
        return false;
    }

    if (exceptionCode != 0) {
        m_lastError = QString("Исключение Modbus: код %1").arg(exceptionCode);
        return false;
    }

    return true;
}

// ============================================================================
//  ФУНКЦИЯ 0x06: WRITE SINGLE REGISTER
// ============================================================================

bool ModbusClient::writeSingleRegister(quint8 slaveId, quint16 addr, quint16 value, int timeoutMs)
{
    if (!isConnected()) {
        m_lastError = "Не подключено";
        return false;
    }

    QByteArray pdu;
    pdu.append(static_cast<char>(MODBUS_FUNC_WRITE_SINGLE_REGISTER));
    pdu.append(static_cast<char>((addr >> 8) & 0xFF));
    pdu.append(static_cast<char>(addr & 0xFF));
    pdu.append(static_cast<char>((value >> 8) & 0xFF));
    pdu.append(static_cast<char>(value & 0xFF));

    QByteArray request;
    if (m_mode == Mode::TCP) {
        request = createMBAPHeader(++m_transactionId, pdu.size() + 1, slaveId);
        request.append(pdu);
    } else if (m_mode == Mode::RTU) {
        request = createRTUFrame(slaveId, pdu);
    } else {
        request = createASCIIFrame(slaveId, pdu);
    }

    QByteArray response;
    if (!sendRequest(request, response, timeoutMs)) {
        return false;
    }

    QByteArray data;
    int exceptionCode = 0;
    if (!parseResponse(response, MODBUS_FUNC_WRITE_SINGLE_REGISTER, data, exceptionCode)) {
        return false;
    }

    if (exceptionCode != 0) {
        m_lastError = QString("Исключение Modbus: код %1").arg(exceptionCode);
        return false;
    }

    return true;
}

// ============================================================================
//  ФУНКЦИЯ 0x0F: WRITE MULTIPLE COILS
// ============================================================================

bool ModbusClient::writeMultipleCoils(quint8 slaveId, quint16 startAddr,
                                      const QVector<bool> &values, int timeoutMs)
{
    if (!isConnected()) {
        m_lastError = "Не подключено";
        return false;
    }

    quint16 quantity = values.size();
    quint8 byteCount = (quantity + 7) / 8;

    QByteArray pdu;
    pdu.append(static_cast<char>(MODBUS_FUNC_WRITE_MULTIPLE_COILS));
    pdu.append(static_cast<char>((startAddr >> 8) & 0xFF));
    pdu.append(static_cast<char>(startAddr & 0xFF));
    pdu.append(static_cast<char>((quantity >> 8) & 0xFF));
    pdu.append(static_cast<char>(quantity & 0xFF));
    pdu.append(static_cast<char>(byteCount));

    // Упаковываем биты в байты
    for (int i = 0; i < byteCount; ++i) {
        quint8 byte = 0;
        for (int j = 0; j < 8; ++j) {
            int bitIndex = i * 8 + j;
            if (bitIndex < quantity && values[bitIndex]) {
                byte |= (1 << j);
            }
        }
        pdu.append(static_cast<char>(byte));
    }

    QByteArray request;
    if (m_mode == Mode::TCP) {
        request = createMBAPHeader(++m_transactionId, pdu.size() + 1, slaveId);
        request.append(pdu);
    } else if (m_mode == Mode::RTU) {
        request = createRTUFrame(slaveId, pdu);
    } else {
        request = createASCIIFrame(slaveId, pdu);
    }

    QByteArray response;
    if (!sendRequest(request, response, timeoutMs)) {
        return false;
    }

    QByteArray data;
    int exceptionCode = 0;
    if (!parseResponse(response, MODBUS_FUNC_WRITE_MULTIPLE_COILS, data, exceptionCode)) {
        return false;
    }

    if (exceptionCode != 0) {
        m_lastError = QString("Исключение Modbus: код %1").arg(exceptionCode);
        return false;
    }

    return true;
}

// ============================================================================
//  ФУНКЦИЯ 0x10: WRITE MULTIPLE REGISTERS
// ============================================================================

bool ModbusClient::writeMultipleRegisters(quint8 slaveId, quint16 startAddr,
                                          const QVector<quint16> &values, int timeoutMs)
{
    if (!isConnected()) {
        m_lastError = "Не подключено";
        return false;
    }

    quint16 quantity = values.size();
    quint8 byteCount = quantity * 2;

    QByteArray pdu;
    pdu.append(static_cast<char>(MODBUS_FUNC_WRITE_MULTIPLE_REGS));
    pdu.append(static_cast<char>((startAddr >> 8) & 0xFF));
    pdu.append(static_cast<char>(startAddr & 0xFF));
    pdu.append(static_cast<char>((quantity >> 8) & 0xFF));
    pdu.append(static_cast<char>(quantity & 0xFF));
    pdu.append(static_cast<char>(byteCount));

    // Добавляем значения регистров (Big-Endian)
    for (quint16 value : values) {
        pdu.append(static_cast<char>((value >> 8) & 0xFF));
        pdu.append(static_cast<char>(value & 0xFF));
    }

    QByteArray request;
    if (m_mode == Mode::TCP) {
        request = createMBAPHeader(++m_transactionId, pdu.size() + 1, slaveId);
        request.append(pdu);
    } else if (m_mode == Mode::RTU) {
        request = createRTUFrame(slaveId, pdu);
    } else {
        request = createASCIIFrame(slaveId, pdu);
    }

    QByteArray response;
    if (!sendRequest(request, response, timeoutMs)) {
        return false;
    }

    QByteArray data;
    int exceptionCode = 0;
    if (!parseResponse(response, MODBUS_FUNC_WRITE_MULTIPLE_REGS, data, exceptionCode)) {
        return false;
    }

    if (exceptionCode != 0) {
        m_lastError = QString("Исключение Modbus: код %1").arg(exceptionCode);
        return false;
    }

    return true;
}

// ============================================================================
//  ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
// ============================================================================

float ModbusClient::registersToFloat(quint16 high, quint16 low)
{
    quint32 bits = (static_cast<quint32>(high) << 16) | low;
    float result;
    std::memcpy(&result, &bits, sizeof(float));
    return result;
}

qint32 ModbusClient::registersToInt32(quint16 high, quint16 low)
{
    return (static_cast<qint32>(high) << 16) | low;
}

quint32 ModbusClient::registersToUint32(quint16 high, quint16 low)
{
    return (static_cast<quint32>(high) << 16) | low;
}

void ModbusClient::floatToRegisters(float value, quint16 &high, quint16 &low)
{
    quint32 bits;
    std::memcpy(&bits, &value, sizeof(float));
    high = (bits >> 16) & 0xFFFF;
    low = bits & 0xFFFF;
}

quint16 ModbusClient::calculateCRC16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<quint8>(data[i]);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

quint8 ModbusClient::calculateLRC(const QByteArray &data)
{
    quint8 lrc = 0;
    for (int i = 0; i < data.size(); ++i) {
        lrc += static_cast<quint8>(data[i]);
    }
    return static_cast<quint8>(-static_cast<qint8>(lrc));
}

// ============================================================================
//  ВНУТРЕННИЕ МЕТОДЫ
// ============================================================================

QByteArray ModbusClient::createMBAPHeader(quint16 transactionId, quint16 length, quint8 unitId)
{
    QByteArray header;
    header.append(static_cast<char>((transactionId >> 8) & 0xFF));
    header.append(static_cast<char>(transactionId & 0xFF));
    header.append(static_cast<char>(0x00));  // Protocol ID (Modbus = 0)
    header.append(static_cast<char>(0x00));
    header.append(static_cast<char>(((length + 1) >> 8) & 0xFF));  // Length (PDU + Unit ID)
    header.append(static_cast<char>((length + 1) & 0xFF));
    header.append(static_cast<char>(unitId));
    return header;
}

QByteArray ModbusClient::createRTUFrame(quint8 slaveId, const QByteArray &pdu)
{
    QByteArray frame;
    frame.append(static_cast<char>(slaveId));
    frame.append(pdu);

    // Добавляем CRC16 (Little-Endian)
    quint16 crc = calculateCRC16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));

    return frame;
}

QByteArray ModbusClient::createASCIIFrame(quint8 slaveId, const QByteArray &pdu)
{
    QByteArray frame;
    frame.append(':');
    frame.append(static_cast<char>(slaveId));
    frame.append(pdu);

    // Добавляем LRC
    quint8 lrc = calculateLRC(frame.mid(1));
    frame.append(static_cast<char>(lrc));

    // Добавляем CR LF
    frame.append('\r');
    frame.append('\n');

    return frame;
}

bool ModbusClient::sendRequest(const QByteArray &request, QByteArray &response, int timeoutMs)
{
    QMutexLocker locker(&m_mutex);
    response.clear();

    // ✅ Выбор транспорта по факту: TCP-сокет или COM-порт
    if (m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        m_tcpSocket->flush();
        while (m_tcpSocket->bytesAvailable() > 0) m_tcpSocket->readAll();

        qint64 bytesWritten = m_tcpSocket->write(request);
        if (bytesWritten != request.size()) {
            m_lastError = "Ошибка отправки запроса (TCP)";
            return false;
        }
        if (!waitForTcpData(timeoutMs)) return false;
        response = m_tcpSocket->readAll();
    } else if (m_serialPort->isOpen()) {
        m_serialPort->flush();
        while (m_serialPort->bytesAvailable() > 0) m_serialPort->readAll();

        qint64 bytesWritten = m_serialPort->write(request);
        if (bytesWritten != request.size()) {
            m_lastError = "Ошибка отправки запроса (COM)";
            return false;
        }
        if (!waitForSerialData(timeoutMs)) return false;
        response = m_serialPort->readAll();
    } else {
        m_lastError = "Нет ни TCP-соединения, ни COM-порта";
        return false;
    }

    return true;
}

bool ModbusClient::parseResponse(const QByteArray &response, quint8 expectedFunction,
                                 QByteArray &data, int &exceptionCode)
{
    exceptionCode = 0;

    if (m_mode == Mode::TCP) {
        // Проверяем MBAP Header (минимум 7 байт)
        if (response.size() < 9) {
            m_lastError = "Ответ слишком короткий";
            return false;
        }

        // Проверяем Protocol ID
        quint16 protocolId = (static_cast<quint8>(response[2]) << 8) |
                             static_cast<quint8>(response[3]);
        if (protocolId != 0) {
            m_lastError = "Неверный Protocol ID";
            return false;
        }

        // Извлекаем Unit ID и Function Code
        quint8 functionCode = static_cast<quint8>(response[7]);

        // Проверяем код исключения (старший бит установлен)
        if (functionCode & 0x80) {
            exceptionCode = static_cast<quint8>(response[8]);
            return true;
        }

        // Проверяем, что функция совпадает
        if (functionCode != expectedFunction) {
            m_lastError = QString("Неверный код функции: ожидался %1, получен %2")
                          .arg(expectedFunction).arg(functionCode);
            return false;
        }

        // Извлекаем данные
        quint8 byteCount = static_cast<quint8>(response[8]);
        if (response.size() < 9 + byteCount) {
            m_lastError = "Недостаточно данных в ответе";
            return false;
        }

        data = response.mid(9, byteCount);

    } else if (m_mode == Mode::RTU) {
        // Проверяем CRC16
        if (response.size() < 4) {
            m_lastError = "Ответ слишком короткий";
            return false;
        }

        QByteArray frameWithoutCrc = response.left(response.size() - 2);
        quint16 receivedCrc = (static_cast<quint8>(response[response.size() - 1]) << 8) |
                              static_cast<quint8>(response[response.size() - 2]);
        quint16 calculatedCrc = calculateCRC16(frameWithoutCrc);

        if (receivedCrc != calculatedCrc) {
            m_lastError = "Ошибка CRC16";
            return false;
        }

        // Извлекаем Function Code
        quint8 functionCode = static_cast<quint8>(response[1]);

        // Проверяем код исключения
        if (functionCode & 0x80) {
            exceptionCode = static_cast<quint8>(response[2]);
            return true;
        }

        // Проверяем, что функция совпадает
        if (functionCode != expectedFunction) {
            m_lastError = QString("Неверный код функции: ожидался %1, получен %2")
                          .arg(expectedFunction).arg(functionCode);
            return false;
        }

        // Извлекаем данные
        quint8 byteCount = static_cast<quint8>(response[2]);
        if (response.size() < 3 + byteCount + 2) {
            m_lastError = "Недостаточно данных в ответе";
            return false;
        }

        data = response.mid(3, byteCount);

    } else if (m_mode == Mode::ASCII) {
        // response = ':' + hex + '\r\n'
        if (response.size() < 5 || response[0] != ':') {
            m_lastError = "Неверный формат ASCII-ответа";
            return false;
        }

        // Отрезать ':' и CRLF
        QByteArray hexPart = response.mid(1);
        while (!hexPart.isEmpty()
               && (hexPart.endsWith('\r') || hexPart.endsWith('\n'))) {
            hexPart.chop(1);
        }
        QByteArray binary = QByteArray::fromHex(hexPart);
        if (binary.size() < 3) {
            m_lastError = "Слишком короткий ASCII-ответ";
            return false;
        }

        // LRC — последний байт binary
        QByteArray frameWithoutLrc = binary.left(binary.size() - 1);
        quint8 receivedLrc = static_cast<quint8>(binary[binary.size() - 1]);
        quint8 calcLrc = calculateLRC(frameWithoutLrc);
        if (receivedLrc != calcLrc) {
            m_lastError = "Ошибка LRC";
            return false;
        }

        // Function Code
        quint8 functionCode = static_cast<quint8>(frameWithoutLrc[1]);

        // Код исключения
        if (functionCode & 0x80) {
            if (frameWithoutLrc.size() < 3) {
                m_lastError = "Слишком короткий ответ с исключением";
                return false;
            }
            exceptionCode = static_cast<quint8>(frameWithoutLrc[2]);
            return true;
        }

        if (functionCode != expectedFunction) {
            m_lastError = QString("Неверный код функции: ожидался %1, получен %2")
                          .arg(expectedFunction).arg(functionCode);
            return false;
        }

        if (frameWithoutLrc.size() < 3) {
            m_lastError = "Слишком короткий ответ";
            return false;
        }
        quint8 byteCount = static_cast<quint8>(frameWithoutLrc[2]);
        if (frameWithoutLrc.size() < 3 + byteCount) {
            m_lastError = "Недостаточно данных в ответе";
            return false;
        }
        data = frameWithoutLrc.mid(3, byteCount);
    }
    return true;
}

bool ModbusClient::waitForTcpData(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        if (m_tcpSocket->waitForReadyRead(100)) {
            // Если уже есть хотя бы 7 байт MBAP — проверить длину
            if (m_tcpSocket->bytesAvailable() >= 7) {
                QByteArray peek = m_tcpSocket->peek(7);
                quint16 len = (static_cast<quint8>(peek[4]) << 8)
                              | static_cast<quint8>(peek[5]);
                // MBAP (7) + PDU(len - 1) = 6 + len всего
                int totalNeeded = 6 + len;
                if (m_tcpSocket->bytesAvailable() >= totalNeeded) {
                    return true;
                }
                // иначе — ждём ещё
            } else {
                return true;   // не Modbus TCP (ASCII через TCP) — отдаём что есть
            }
        }
    }

    m_lastError = "Таймаут ожидания ответа (TCP)";
    return false;
}

bool ModbusClient::waitForSerialData(int timeoutMs)
{
    if (!m_serialPort->waitForReadyRead(timeoutMs)) {
        m_lastError = "Таймаут ожидания ответа";
        return false;
    }

    return true;
}

bool ModbusClient::connectAscii(const QString &portName, int baudRate,
                                QSerialPort::DataBits dataBits,
                                QSerialPort::Parity parity,
                                QSerialPort::StopBits stopBits,
                                int timeoutMs)
{
    QMutexLocker locker(&m_mutex);
    Q_UNUSED(timeoutMs);

    disconnect();
    m_mode = Mode::ASCII;

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(dataBits);
    m_serialPort->setParity(parity);
    m_serialPort->setStopBits(stopBits);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        m_lastError = "Не удалось открыть COM-порт " + portName;
        qWarning() << "[ModbusClient]" << m_lastError;
        return false;
    }

    qDebug() << "[ModbusClient] Открыт COM-порт" << portName
             << "скорость" << baudRate << "(ASCII)";
    emit connected();
    return true;
}

bool ModbusClient::connectAsciiTcp(const QString &ip, quint16 port, int timeoutMs)
{
    QMutexLocker locker(&m_mutex);

    disconnect();
    m_mode = Mode::ASCII;   // логика sendRequest для ASCII одинакова для TCP/COM

    m_tcpSocket->connectToHost(ip, port);
    if (!m_tcpSocket->waitForConnected(timeoutMs)) {
        m_lastError = "Не удалось подключиться к " + ip + ":" + QString::number(port);
        return false;
    }

    qDebug() << "[ModbusClient] Подключено к" << ip << ":" << port << "(ASCII/TCP)";
    emit connected();
    return true;
}
