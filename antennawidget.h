#ifndef ANTENNAWIDGET_H
#define ANTENNAWIDGET_H
#include "basedocwidget.h"
#include <QJsonObject>
namespace Ui { class AntennaForm; }
class AntennaWidget : public BaseDocWidget
{
    Q_OBJECT
public:
    explicit AntennaWidget(QWidget *parent = nullptr);
    ~AntennaWidget();
    void loadFromFile(const QString &path) override;
    void saveToFile() override;
    QSize documentFixedSize() const override;
    DocType docType() const override { return DocType::Antenna; }
    void processDocClientData(const QJsonObject &data) override;
    // ✅ ОБЯЗАТЕЛЬНО ДОБАВИТЬ ЭТИ ОБЪЯВЛЕНИЯ:
    QJsonObject getDocumentParameters() const override;
    void setDocumentParameters(const QJsonObject &params) override;

private slots:
    void onFieldChanged();
    void onApplyClicked();
private:
    Ui::AntennaForm *ui;
};
#endif
