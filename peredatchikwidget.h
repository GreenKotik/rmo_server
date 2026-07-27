#ifndef PEREDATCHIKWIDGET_H
#define PEREDATCHIKWIDGET_H
#include "basedocwidget.h"
#include <QJsonObject>
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
    // ✅ ОБЯЗАТЕЛЬНО ДОБАВИТЬ ЭТИ ОБЪЯВЛЕНИЯ:
    QJsonObject getDocumentParameters() const override;
    void setDocumentParameters(const QJsonObject &params) override;
private slots:
    void onFieldChanged();
    void onApplyClicked();
private:
    Ui::PeredatchikForm *ui;
};
#endif
