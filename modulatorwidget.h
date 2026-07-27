#ifndef MODULATORWIDGET_H
#define MODULATORWIDGET_H
#include "basedocwidget.h"
#include <QJsonObject>
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
    // ✅ ОБЯЗАТЕЛЬНО ДОБАВИТЬ ЭТИ ОБЪЯВЛЕНИЯ:
    QJsonObject getDocumentParameters() const override;
    void setDocumentParameters(const QJsonObject &params) override;
private slots:
    void onFieldChanged();
    void onApplyClicked();
private:
    Ui::ModulatorForm *ui;
};
#endif
