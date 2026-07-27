#ifndef TCPASCIICLIENT_H
#define TCPASCIICLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>
#include <QByteArray>
#include <QMutex>
#include <QDebug>

// ============================================================================
// ✅ ТЕРМИНАТОРЫ КОМАНД
// ============================================================================
namespace Terminator {
    const QByteArray CR   = QByteArray(1, '\r');           // 0x0D - Varian, CPI, Xicom
    const QByteArray LF   = QByteArray(1, '\n');           // 0x0A - Agilent
    const QByteArray CRLF = QByteArray("\r\n");            // 0x0D 0x0A
    const QByteArray ETX  = QByteArray(1, '\x03');         // 0x03 - Xicom
    const QByteArray NONE = QByteArray();                  // Без терминатора
}

// ============================================================================
// ✅ ТИПЫ КОНТРОЛЬНЫХ СУММ
// ============================================================================
enum class ChecksumType {
    None,           // Без контрольной суммы
    XOR,            // XOR всех байт (Varian, Xicom)
    SumMod256,      // (sum % 256) + 32 (CPI)
    SumByte,        // Простая сумма байт (Paradise, Peak)
    CRC8,           // CRC-8 (Dallas/Maxim)
    CRC16_Modbus,   // CRC-16 Modbus
    CRC16_CCITT     // CRC-16 CCITT
};

// ============================================================================
// ✅ TCP/ASCII КЛИЕНТ ДЛЯ СПЕЦИФИЧНЫХ ПЕРЕДАТЧИКОВ
// Поддерживает: Varian, CPI, Xicom, Paradise, Peak, Agilent, Maxtech
// ============================================================================
class TcpAsciiClient : public QObject
{
    Q_OBJECT
public:
    explicit TcpAsciiClient(QObject *parent = nullptr);
    ~TcpAsciiClient();

    // =========================================================================
    // ✅ ПОДКЛЮЧЕНИЕ / ОТКЛЮЧЕНИЕ
    // =========================================================================

    // Подключение к устройству
    bool connect(const QString &ip, quint16 port, int timeoutMs = 2000);

    // Отключение
    void disconnect();

    // Проверка состояния
    bool isConnected() const;

    // =========================================================================
    // ✅ ОТПРАВКА И ПРИЁМ ДАННЫХ
    // =========================================================================

    // Отправить сырые данные
    bool sendRaw(const QByteArray &data);

    // Отправить команду с терминатором
    bool sendCommand(const QByteArray &command, const QByteArray &terminator = Terminator::CR);

    // Отправить команду и ждать ответ заданной длины
    QByteArray sendAndWait(const QByteArray &command,
                           int expectedMinLength,
                           int timeoutMs = 1000,
                           const QByteArray &terminator = Terminator::CR);

    // Отправить команду и ждать ответ до терминатора
    QByteArray sendAndWaitForTerminator(const QByteArray &command,
                                        const QByteArray &terminator = Terminator::CR,
                                        int timeoutMs = 1000);

    // Просто прочитать доступные данные
    QByteArray readAvailable(int timeoutMs = 500);

    // Очистить буфер приёма
    void clearBuffer();

    // =========================================================================
    // ✅ КОНТРОЛЬНЫЕ СУММЫ
    // =========================================================================

    // Рассчитать контрольную сумму для данных
    static quint16 calculateChecksum(const QByteArray &data, ChecksumType type);

    // Добавить контрольную сумму к данным (возвращает новые данные)
    static QByteArray appendChecksum(const QByteArray &data, ChecksumType type);

    // Проверить контрольную сумму (последние N байт = контрольная сумма)
    static bool verifyChecksum(const QByteArray &data, ChecksumType type, int checksumLength = 1);

    // =========================================================================
    // ✅ ФОРМАТИРОВАНИЕ ДАННЫХ
    // =========================================================================

    // Конвертация hex-строки в байты ("48656C6C6F" -> "Hello")
    static QByteArray hexToBytes(const QString &hex);

    // Конвертация байтов в hex-строку ("Hello" -> "48656C6C6F")
    static QString bytesToHex(const QByteArray &data);

    // Конвертация байтов в читаемый вид (для логов)
    static QString bytesToReadable(const QByteArray &data);

    // =========================================================================
    // ✅ ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
    // =========================================================================

    // Получить последнюю ошибку
    QString lastError() const { return m_lastError; }

    // Установить таймаут по умолчанию
    void setDefaultTimeout(int timeoutMs) { m_defaultTimeout = timeoutMs; }

    // Получить QTcpSocket для продвинутого использования
    QTcpSocket* socket() { return m_socket; }

signals:
    void connected();
    void disconnected();
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &error);

private:
    // =========================================================================
    // ✅ ВНУТРЕННИЕ МЕТОДЫ
    // =========================================================================

    // Ожидание данных с таймаутом
    bool waitForData(int timeoutMs);

    // Ожидание минимального количества байт
    bool waitForBytes(int minBytes, int timeoutMs);

    QTcpSocket *m_socket;
    QMutex m_mutex;
    QString m_lastError;
    int m_defaultTimeout;
};

#endif // TCPASCIICLIENT_H
