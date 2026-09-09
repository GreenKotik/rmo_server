#ifndef MODULATORTAB_H
#define MODULATORTAB_H

#include "modulatortabbase.h"
#include "ui_modulatortab.h"

// ============================================================================
// ТАБ ДЛЯ УПРАВЛЕНИЯ ОДНИМ МОДУЛЯТОРОМ
// slotIndex: 1 = Modulator 1, 2 = Modulator 2
// ============================================================================
class ModulatorTab : public ModulatorTabBase
{
    Q_OBJECT
public:
    explicit ModulatorTab(BaseDocWidget *docWidget, int slotIndex, QWidget *parent = nullptr);
    ~ModulatorTab() override;

    void refreshData() override;

private slots:
    void onTurnOnClicked();
    void onTurnOffClicked();
    void onResetAlarmsClicked();

private:
    Ui::ModulatorTabForm *ui;
    int m_slotIndex;
    int m_deviceId;
    QString m_deviceName;
};

#endif // MODULATORTAB_H
