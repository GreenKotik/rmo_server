#ifndef MODULATORWIDGET_H
#define MODULATORWIDGET_H

#include "basedocwidget.h"
#include "devicedata.h"       // ✅ Для ModulatorData
#include "ledindicator.h"     // ✅ Для LedIndicator
#include "ringindicator.h"    // ✅ Для RingIndicator
#include <QJsonObject>
#include <memory>

namespace Ui { class ModulatorForm; }
class ModulatorControlDialog;  // ✅ Forward declaration

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
    // ✅ НОВЫЙ МЕТОД: Открыть диалог управления модулятором
    void openControlDialog();

protected:
    // ✅ Переопределяем контекстное меню для добавления пункта "Управление"
    void contextMenuEvent(QContextMenuEvent *event) override;
    // ✅ Переопределяем для получения данных от драйвера
    void processPollResult(int chainId, int deviceIndex, std::shared_ptr<IDeviceData> data) override;

private slots:
    void onFieldChanged();
    void onApplyClicked();
    void onOpenControlDialog();  // ✅ ДОБАВЛЕНО

private:
    Ui::ModulatorForm *ui;
    ModulatorControlDialog *m_controlDialog = nullptr;  // ✅ НОВОЕ ПОЛЕ

    // ✅ Метод обновления UI из унифицированных данных модулятора
    void updateUIFromModulatorData(const std::shared_ptr<ModulatorData> &data);
};

#endif // MODULATORWIDGET_H
