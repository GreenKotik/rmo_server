#ifndef SERIALPORTCLIENT_H
#define SERIALPORTCLIENT_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QDebug>

class SerialPortClient : public QObject
{
    Q_OBJECT
public:
    explicit SerialPortClient(QObject *parent = nullptr);
    ~SerialPortClient();

    bool open(const QString &portName, int baudRate, int dataBits,
              const QString &parity, int stopBits);
    void close();
    bool isOpen() const;

    bool writeData(const QByteArray &data);
    bool writeAndRead(const QByteArray &data, QByteArray &response, int timeoutMs = 1000);

    QString lastError() const { return m_lastError; }

private slots:
    void onReadyRead();

private:
    QSerialPort *m_serialPort;
    QByteArray m_readBuffer;
    QString m_lastError;

signals:
    void dataReceived();
};

#endif // SERIALPORTCLIENT_H
