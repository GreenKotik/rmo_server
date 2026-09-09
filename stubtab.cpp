#include "stubtab.h"

StubTab::StubTab(BaseDocWidget *docWidget, const QString &title, QWidget *parent)
    : ModulatorTabBase(docWidget, parent)
    , ui(new Ui::StubTabForm)
{
    ui->setupUi(this);

    ui->labelTitle->setText(QString("<b>%1</b>").arg(title));
}

StubTab::~StubTab()
{
    delete ui;
}
