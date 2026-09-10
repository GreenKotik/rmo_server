#ifndef PEREDATCHIKWIDGET_H
#define PEREDATCHIKWIDGET_H

#include "basedocwidget.h"
#include "devicedata.h"       // ✅ Для Boreas3Data, TransmitterData
#include "databuffer.h"       // ✅ Для DoubleBuffer
#include <QJsonObject>
#include <memory>

namespace Ui { class PeredatchikForm; }

class PeredatchikWidget : public BaseDocWidget
{
    Q_OBJECT
public:
    explicit PeredatchikWidget(QWidget *parent = nullptr);
    ~PeredatchikWidget();

    void loadFromFile(const QString &path) override;
    void saveToFile() override;
    QSize documentFixedSize() const override;
    DocType docType() const override { return DocType::Transmitter; }
    void processDocClientData(const QJsonObject &data) override;

    QJsonObject getDocumentParameters() const override;
    void setDocumentParameters(const QJsonObject &params) override;

protected:
    // ✅ 1. Перехватываем данные от драйвера опроса
    void processPollResult(int chainId, int deviceIndex, std::shared_ptr<IDeviceData> data) override;

private slots:
    void onFieldChanged();
    void onApplyClicked();

    // ✅ 2. Цепочка обработки данных (вызывается синхронно после опроса)
    void processDataChain();

private:
    Ui::PeredatchikForm *ui;

    // ✅ Двойной буфер для агрегации данных передатчика
    DoubleBuffer<TransmitterData> m_dataBuffer;
};

#endif // PEREDATCHIKWIDGET_H
