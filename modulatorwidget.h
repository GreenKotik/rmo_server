#ifndef MODULATORWIDGET_H
#define MODULATORWIDGET_H

#include "basedocwidget.h"
#include "devicedata.h"
#include <QJsonObject>
#include <memory>

namespace Ui { class ModulatorForm; }

class ModulatorWidget : public BaseDocWidget
{
    Q_OBJECT
public:
    explicit ModulatorWidget(QWidget *parent = nullptr);
    ~ModulatorWidget();

    void loadFromFile(const QString &path) override;
    void saveToFile() override;
    QSize documentFixedSize() const override;
    DocType docType() const override { return DocType::Modulator; }
    void processDocClientData(const QJsonObject &data) override;

    QJsonObject getDocumentParameters() const override;
    void setDocumentParameters(const QJsonObject &params) override;

protected:
    // ✅ Переопределяем для обновления UI при получении данных
    void processPollResult(int chainId, int deviceIndex, std::shared_ptr<IDeviceData> data) override;

private slots:
    void onFieldChanged();
    void onApplyClicked();

private:
    Ui::ModulatorForm *ui;

    // ✅ Метод обновления UI из данных модулятора
    void updateUIFromModulatorData(const std::shared_ptr<ModulatorData> &data);
};

#endif // MODULATORWIDGET_H
