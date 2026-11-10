#ifndef MODULATORTAB_H
#define MODULATORTAB_H

#include "modulatortabbase.h"
#include "modulatorconfig.h"
#include "deviceids.h"
#include "ui_modulatortab.h"

class ModulatorData;

// ============================================================================
// ТАБ ДЛЯ УПРАВЛЕНИЯ ОДНИМ МОДУЛЯТОРОМ
// slotIndex: 1 = Modulator 1, 2 = Modulator 2
//
// Таблица «параметр → значение» строится из конфига модели модулятора
// (drivers/modulators/*.json) по deviceId. Подпись — label (если пуст — key).
// ============================================================================
class ModulatorTab : public ModulatorTabBase
{
    Q_OBJECT
public:
    explicit ModulatorTab(BaseDocWidget *docWidget, int slotIndex, QWidget *parent = nullptr);
    ~ModulatorTab() override;

    void refreshData() override;

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onTurnOnClicked();
    void onTurnOffClicked();
    void onResetAlarmsClicked();

private:
    void loadConfigForDevice();                         // перечитать конфиг по m_deviceId
    void rebuildValuesTable();                          // строки по числу params
    void refreshTableValues(const ModulatorData &data); // значения по target

    Ui::ModulatorTabForm *ui;
    int m_slotIndex;
    int m_deviceId = ID_UNKNOWN;
    QString m_deviceName;

    ModulatorConfig m_config;
    bool m_configLoaded = false;
};

#endif // MODULATORTAB_H
