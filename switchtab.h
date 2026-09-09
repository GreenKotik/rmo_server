#ifndef SWITCHTAB_H
#define SWITCHTAB_H

#include "modulatortabbase.h"
#include "ui_switchtab.h"

// ============================================================================
// ТАБ ДЛЯ УПРАВЛЕНИЯ ПЕРЕКЛЮЧАТЕЛЕМ РЕЗЕРВИРОВАНИЯ (slotIndex = 3)
// ============================================================================
class SwitchTab : public ModulatorTabBase
{
    Q_OBJECT
public:
    explicit SwitchTab(BaseDocWidget *docWidget, QWidget *parent = nullptr);
    ~SwitchTab() override;

    void refreshData() override;

private slots:
    void onSwitchToMod1Clicked();
    void onSwitchToMod2Clicked();
    void onSetAutoModeClicked();
    void onSetManualModeClicked();

private:
    Ui::SwitchTabForm *ui;
    int m_slotIndex;
    int m_deviceId;
    QString m_deviceName;
};

#endif // SWITCHTAB_H
