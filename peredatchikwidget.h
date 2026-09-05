#ifndef PEREDATCHIKWIDGET_H
#define PEREDATCHIKWIDGET_H

#include "basedocwidget.h"
#include "ui_peredatchikform.h"
#include "devicedata.h" // ✅ Чтобы видеть Boreas3Data
#include <memory>

class PeredatchikWidget : public BaseDocWidget
{
    Q_OBJECT
public:
    explicit PeredatchikWidget(QWidget *parent = nullptr);
    ~PeredatchikWidget() override;

    void loadFromFile(const QString &path) override;
    void saveToFile() override;
    QSize documentFixedSize() const override;
    DocType docType() const override { return DocType::Transmitter; }
    void processDocClientData(const QJsonObject &data) override;

protected:
    // ✅ Переопределяем метод пакетного обновления UI
    void updateUIFromBuffer() override;

private:
    Ui::PeredatchikForm *ui;
};

#endif // PEREDATCHIKWIDGET_H
