#include "serialportclient.h"
#include <QEventLoop>

SerialPortClient::SerialPortClient(QObject *parent)
    : QObject(parent)
    , m_serialPort(new QSerialPort(this))
{
    connect(m_serialPort, &QSerialPort::readyRead, this, &SerialPortClient::onReadyRead);
}

SerialPortClient::~SerialPortClient()
{
    close();
}

bool SerialPortClient::open(const QString &portName, int baudRate, int dataBits,
                            const QString &parity, int stopBits)
{
    close();

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(static_cast<QSerialPort::DataBits>(dataBits));

    if (parity == "Even") {
        m_serialPort->setParity(QSerialPort::EvenParity);
    } else if (parity == "Odd") {
        m_serialPort->setParity(QSerialPort::OddParity);
    } else {
        m_serialPort->setParity(QSerialPort::NoParity);
    }

    m_serialPort->setStopBits(static_cast<QSerialPort::StopBits>(stopBits));

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        m_lastError = m_serialPort->errorString();
        qWarning() << "[SerialPortClient] Ошибка открытия порта:" << m_lastError;
        return false;
    }

    qDebug() << "[SerialPortClient] Порт открыт:" << portName;
    return true;
}

void SerialPortClient::close()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }
    m_readBuffer.clear();
}

bool SerialPortClient::isOpen() const
{
    return m_serialPort->isOpen();
}

bool SerialPortClient::writeData(const QByteArray &data)
{
    if (!m_serialPort->isOpen()) {
        m_lastError = "Порт не открыт";
        return false;
    }

    qint64 bytesWritten = m_serialPort->write(data);
    if (bytesWritten != data.size()) {
        m_lastError = "Ошибка записи данных";
        return false;
    }

    m_serialPort->flush();
    return true;
}

bool SerialPortClient::writeAndRead(const QByteArray &data, QByteArray &response, int timeoutMs)
{
    m_readBuffer.clear();

    if (!writeData(data)) {
        return false;
    }

    QEventLoop loop;
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    connect(this, &SerialPortClient::dataReceived, &loop, &QEventLoop::quit, Qt::UniqueConnection);
    loop.exec();

    response = m_readBuffer;
    return !response.isEmpty();
}

void SerialPortClient::onReadyRead()
{
    m_readBuffer.append(m_serialPort->readAll());
    emit dataReceived();
}
