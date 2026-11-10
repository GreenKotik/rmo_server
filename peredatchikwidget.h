#ifndef PEREDATCHIKWIDGET_H
#define PEREDATCHIKWIDGET_H

#include "basedocwidget.h"
#include "devicedata.h"       // ✅ Для Boreas3Data, TransmitterData
#include "databuffer.h"       // ✅ Для DoubleBuffer
#include <QJsonObject>
#include <memory>
#include <QVector>
#include <QString>

// ============================================================================
// ✅ Структура хранения настроек одного датчика температуры
// (кэш из JSON-файла документа, читается один раз и обновляется при
// перезагрузке настроек через onSettingsReloaded())
// ============================================================================
struct PeredatchikTempConfig {
    QString name;             // Имя датчика (для отображения в UI)
    QString type;             // "Нет", "Температура", "Температура и влажность"
    bool active = false;      // true, если type != "Нет"
};

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
    // ✅ Переопределяем: вызывается при завершении Fast Pool.
    //    Здесь — запись в БД и рассылка клиентам (один раз за цикл).
    void updateUIFromBuffer() override;
    void onSettingsReloaded() override;
    void onFileLoaded() override;

private slots:
    void onFieldChanged();
    void onApplyClicked();

private:
    Ui::PeredatchikForm *ui;

    // ✅ Двойной буфер для агрегации данных передатчика
    DoubleBuffer<TransmitterData> m_dataBuffer;
    // ✅ Кэш настроек датчиков температуры (4 шт.)
    QVector<PeredatchikTempConfig> m_tempConfigs;

    // ✅ Метод для чтения настроек из m_fullDocument
    void loadTemperatureConfigs();
};

#endif // PEREDATCHIKWIDGET_H
