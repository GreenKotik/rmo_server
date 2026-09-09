#ifndef STUBTAB_H
#define STUBTAB_H

#include "modulatortabbase.h"
#include "ui_stubtab.h"

// ============================================================================
// ТАБ-ЗАГЛУШКА (для устройств без параметров управления)
// ============================================================================
class StubTab : public ModulatorTabBase
{
    Q_OBJECT
public:
    explicit StubTab(BaseDocWidget *docWidget, const QString &title, QWidget *parent = nullptr);
    ~StubTab() override;

    void refreshData() override {}

private:
    Ui::StubTabForm *ui;
};

#endif // STUBTAB_H
