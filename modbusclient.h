#ifndef MODBUSCLIENT_H
#define MODBUSCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QSerialPort>
#include <QMutex>
#include <QVector>
#include <QByteArray>
#include <QDebug>

// ============================================================================
// ✅ УНИВЕРСАЛЬНЫЙ MODBUS-КЛИЕНТ (TCP + RTU + ASCII)
// Поддержка функций: 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0F, 0x10
// ============================================================================
class ModbusClient : public QObject
{
    Q_OBJECT
public:
    // Режимы работы
    enum class Mode { TCP, RTU, ASCII };

    explicit ModbusClient(QObject *parent = nullptr);
    ~ModbusClient();

    // =========================================================================
    // ✅ ПОДКЛЮЧЕНИЕ
    // =========================================================================

    // Modbus TCP
    bool connectTcp(const QString &ip, quint16 port = 502, int timeoutMs = 1000);

    // Modbus RTU (через COM-порт)
    bool connectRtu(const QString &portName, int baudRate = 9600,
                    QSerialPort::DataBits dataBits = QSerialPort::Data8,
                    QSerialPort::Parity parity = QSerialPort::NoParity,
                    QSerialPort::StopBits stopBits = QSerialPort::OneStop,
                    int timeoutMs = 1000);

    // Отключение
    void disconnect();

    // Проверка состояния
    bool isConnected() const;

    // Получить последнюю ошибку
    QString lastError() const { return m_lastError; }

    // =========================================================================
    // ✅ ФУНКЦИИ ЧТЕНИЯ
    // =========================================================================

    // 0x01: Read Coils (чтение битовых выходов)
    bool readCoils(quint8 slaveId, quint16 startAddr, quint16 quantity,
                   QVector<bool> &values, int timeoutMs = 1000);

    // 0x02: Read Discrete Inputs (чтение битовых входов)
    bool readDiscreteInputs(quint8 slaveId, quint16 startAddr, quint16 quantity,
                            QVector<bool> &values, int timeoutMs = 1000);

    // 0x03: Read Holding Registers (чтение регистров хранения)
    bool readHoldingRegisters(quint8 slaveId, quint16 startAddr, quint16 quantity,
                              QVector<quint16> &values, int timeoutMs = 1000);

    // 0x04: Read Input Registers (чтение входных регистров)
    bool readInputRegisters(quint8 slaveId, quint16 startAddr, quint16 quantity,
                            QVector<quint16> &values, int timeoutMs = 1000);

    // =========================================================================
    // ✅ ФУНКЦИИ ЗАПИСИ
    // =========================================================================

    // 0x05: Write Single Coil (запись одного бита)
    bool writeSingleCoil(quint8 slaveId, quint16 addr, bool value, int timeoutMs = 1000);

    // 0x06: Write Single Register (запись одного регистра)
    bool writeSingleRegister(quint8 slaveId, quint16 addr, quint16 value, int timeoutMs = 1000);

    // 0x0F: Write Multiple Coils (запись нескольких битов)
    bool writeMultipleCoils(quint8 slaveId, quint16 startAddr, const QVector<bool> &values,
                            int timeoutMs = 1000);

    // 0x10: Write Multiple Registers (запись нескольких регистров)
    bool writeMultipleRegisters(quint8 slaveId, quint16 startAddr, const QVector<quint16> &values,
                                int timeoutMs = 1000);

    // =========================================================================
    // ✅ ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ (статические)
    // =========================================================================

    // Преобразование 2 регистров в float (Big-Endian, как в OVEN/WB-MAP)
    static float registersToFloat(quint16 high, quint16 low);

    // Преобразование 2 регистров в qint32 (Big-Endian)
    static qint32 registersToInt32(quint16 high, quint16 low);

    // Преобразование 2 регистров в quint32 (Big-Endian)
    static quint32 registersToUint32(quint16 high, quint16 low);

    // Преобразование float в 2 регистра (Big-Endian)
    static void floatToRegisters(float value, quint16 &high, quint16 &low);

    // Расчёт CRC16 для Modbus RTU (полином 0xA001)
    static quint16 calculateCRC16(const QByteArray &data);

    // Расчёт LRC для Modbus ASCII
    static quint8 calculateLRC(const QByteArray &data);
    // ✅ Modbus ASCII через COM-порт
    bool connectAscii(const QString &portName, int baudRate = 9600,
                      QSerialPort::DataBits dataBits = QSerialPort::Data8,
                      QSerialPort::Parity parity = QSerialPort::NoParity,
                      QSerialPort::StopBits stopBits = QSerialPort::OneStop,
                      int timeoutMs = 1000);

    // ✅ Modbus ASCII через TCP (редко, но встречается)
    bool connectAsciiTcp(const QString &ip, quint16 port = 502, int timeoutMs = 1000);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);

private:
    // =========================================================================
    // ✅ ВНУТРЕННИЕ МЕТОДЫ
    // =========================================================================

    // Отправка запроса и получение ответа (универсальный метод)
    bool sendRequest(const QByteArray &request, QByteArray &response, int timeoutMs);

    // Создание MBAP Header для TCP
    QByteArray createMBAPHeader(quint16 transactionId, quint16 length, quint8 unitId);

    // Создание RTU-фрейма (Slave ID + PDU + CRC16)
    QByteArray createRTUFrame(quint8 slaveId, const QByteArray &pdu);

    // Создание ASCII-фрейма (':' + Slave ID + PDU + LRC + CR LF)
    QByteArray createASCIIFrame(quint8 slaveId, const QByteArray &pdu);

    // Парсинг ответа
    bool parseResponse(const QByteArray &response, quint8 expectedFunction,
                       QByteArray &data, int &exceptionCode);

    // Проверка кода исключения
    bool checkExceptionCode(quint8 functionCode, quint8 exceptionCode);

    // Ожидание данных от TCP
    bool waitForTcpData(int timeoutMs);

    // Ожидание данных от COM-порта
    bool waitForSerialData(int timeoutMs);

    Mode m_mode;
    QTcpSocket *m_tcpSocket;
    QSerialPort *m_serialPort;
    QMutex m_mutex;
    quint16 m_transactionId;
    QString m_lastError;
};

#endif // MODBUSCLIENT_H
