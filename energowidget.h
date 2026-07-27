#ifndef ENERGOEWIDGET_H
#define ENERGOEWIDGET_H
#include "basedocwidget.h"
#include <QJsonObject>
namespace Ui { class EnergoForm; }
class EnergoWidget : public BaseDocWidget
{
    Q_OBJECT
public:
    explicit EnergoWidget(QWidget *parent = nullptr);
    ~EnergoWidget();
    void loadFromFile(const QString &path) override;
    void saveToFile() override;
    QSize documentFixedSize() const override;
    DocType docType() const override { return DocType::Energo; }
    void processDocClientData(const QJsonObject &data) override;
    // ✅ ОБЯЗАТЕЛЬНО ДОБАВИТЬ ЭТИ ОБЪЯВЛЕНИЯ:
    QJsonObject getDocumentParameters() const override;
    void setDocumentParameters(const QJsonObject &params) override;
private slots:
    void onFieldChanged();
    void onApplyClicked();
private:
    Ui::EnergoForm *ui;
};

#endif // ENERGOEWIDGET_H
