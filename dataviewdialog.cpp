#include "dataviewdialog.h"
#include "ui_dataviewdialog.h"
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDateTime>
#include <QFileInfo>
#include <QDebug>

DataViewDialog::DataViewDialog(const QString &filePath, int docType, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DataViewDialog)
    , m_filePath(filePath)
    , m_docType(docType)
{
    ui->setupUi(this);
    setWindowTitle("Data View: " + QFileInfo(filePath).fileName());

    ui->dateStart->setDate(QDate::currentDate().addDays(-7));
    ui->dateEnd->setDate(QDate::currentDate());

    connect(ui->btnLoad, &QPushButton::clicked, this, &DataViewDialog::onLoadDataClicked);
    connect(ui->btnExport, &QPushButton::clicked, this, &DataViewDialog::onExportClicked);
    connect(ui->treeChannels, &QTreeWidget::itemChanged, this, &DataViewDialog::onTreeItemChanged);

    setupGraph();
    setupTreeChannels();
    loadDataFromDB();
}

DataViewDialog::~DataViewDialog()
{
    delete ui;
}

void DataViewDialog::setupGraph()
{
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("dd.MM\nhh:mm");
    ui->graph->xAxis->setTicker(dateTicker);
    ui->graph->xAxis->setLabel("Time");
    ui->graph->yAxis->setLabel("Value");
    ui->graph->xAxis->grid()->setVisible(true);
    ui->graph->yAxis->grid()->setVisible(true);
    ui->graph->legend->setVisible(true);
    ui->graph->legend->setFont(QFont("Arial", 8));
    ui->graph->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
}

void DataViewDialog::setupTreeChannels()
{
    ui->treeChannels->clear();
    ui->treeChannels->setHeaderLabel("Channels");

    if (m_docType == 1) {
        QTreeWidgetItem *base = new QTreeWidgetItem(ui->treeChannels, {"Base params"});
        base->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *az = new QTreeWidgetItem(base, {"Azimuth"});
        az->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *el = new QTreeWidgetItem(base, {"Elevation"});
        el->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *temp = new QTreeWidgetItem(ui->treeChannels, {"Temperatures"});
        temp->setCheckState(0, Qt::Unchecked);
        for (int i = 1; i <= 8; ++i) {
            QTreeWidgetItem *t = new QTreeWidgetItem(temp, {QString("Temperature%1").arg(i)});
            t->setCheckState(0, Qt::Unchecked);
        }

        QTreeWidgetItem *ibp = new QTreeWidgetItem(ui->treeChannels, {"IBP-1"});
        ibp->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *iv = new QTreeWidgetItem(ibp, {"IBP1_V"});
        iv->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *ia = new QTreeWidgetItem(ibp, {"IBP1_A1"});
        ia->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *iw = new QTreeWidgetItem(ibp, {"IBP1_W1"});
        iw->setCheckState(0, Qt::Unchecked);
    }
    else if (m_docType == 2) {
        QTreeWidgetItem *per = new QTreeWidgetItem(ui->treeChannels, {"Transmitter 1"});
        per->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *pi = new QTreeWidgetItem(per, {"PerPowerIn1"});
        pi->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *po = new QTreeWidgetItem(per, {"PerPower1"});
        po->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *pr = new QTreeWidgetItem(per, {"PerPowerOtr1"});
        pr->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *at = new QTreeWidgetItem(per, {"Attenuator1"});
        at->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *sp = new QTreeWidgetItem(per, {"Spiral1"});
        sp->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *temp = new QTreeWidgetItem(ui->treeChannels, {"Temperatures"});
        temp->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *t1 = new QTreeWidgetItem(temp, {"Temper1"});
        t1->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *t2 = new QTreeWidgetItem(temp, {"Temper2"});
        t2->setCheckState(0, Qt::Unchecked);
    }
    else if (m_docType == 3) {
        for (int vv = 1; vv <= 4; ++vv) {
            QTreeWidgetItem *vvIn = new QTreeWidgetItem(ui->treeChannels, {QString("VV%1 Input").arg(vv)});
            vvIn->setCheckState(0, Qt::Unchecked);

            QTreeWidgetItem *v1 = new QTreeWidgetItem(vvIn, {QString("VV%1_In_V1").arg(vv)});
            v1->setCheckState(0, Qt::Unchecked);

            QTreeWidgetItem *v2 = new QTreeWidgetItem(vvIn, {QString("VV%1_In_V2").arg(vv)});
            v2->setCheckState(0, Qt::Unchecked);

            QTreeWidgetItem *v3 = new QTreeWidgetItem(vvIn, {QString("VV%1_In_V3").arg(vv)});
            v3->setCheckState(0, Qt::Unchecked);
        }
        for (int ibp = 1; ibp <= 3; ++ibp) {
            QTreeWidgetItem *ibpItem = new QTreeWidgetItem(ui->treeChannels, {QString("IBP-%1").arg(ibp)});
            ibpItem->setCheckState(0, Qt::Unchecked);

            QTreeWidgetItem *bv = new QTreeWidgetItem(ibpItem, {QString("IBP%1_Bat_V").arg(ibp)});
            bv->setCheckState(0, Qt::Unchecked);

            QTreeWidgetItem *ba = new QTreeWidgetItem(ibpItem, {QString("IBP%1_Bat_A").arg(ibp)});
            ba->setCheckState(0, Qt::Unchecked);

            QTreeWidgetItem *bz = new QTreeWidgetItem(ibpItem, {QString("IBP%1_Bat_Zarad").arg(ibp)});
            bz->setCheckState(0, Qt::Unchecked);
        }
    }
    else if (m_docType == 4) {
        QTreeWidgetItem *mod = new QTreeWidgetItem(ui->treeChannels, {"Modulator"});
        mod->setCheckState(0, Qt::Unchecked);

        QTreeWidgetItem *st = new QTreeWidgetItem(mod, {"Status"});
        st->setCheckState(0, Qt::Unchecked);
    }

    ui->treeChannels->expandAll();
}

void DataViewDialog::onLoadDataClicked()
{
    ui->statusLabel->setText("Loading...");
    ui->progress->setVisible(true);
    ui->progress->setValue(0);
    loadDataFromDB();
}

void DataViewDialog::onExportClicked()
{
    ui->statusLabel->setText("Export...");
}

void DataViewDialog::onTreeItemChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(item);
    Q_UNUSED(column);
    updateGraph();
}

void DataViewDialog::loadDataFromDB()
{
    QString safeTableName = getSafeTableName(m_filePath);

    QDateTime startDt(ui->dateStart->date(), QTime(0, 0, 0));
    QDateTime endDt(ui->dateEnd->date(), QTime(23, 59, 59));

    QSqlQuery query;
    query.prepare(QString("SELECT * FROM \"%1\" "
                          "WHERE \"Дата_и_время\" >= :start AND \"Дата_и_время\" <= :end "
                          "ORDER BY \"Дата_и_время\" ASC")
                  .arg(safeTableName));
    query.bindValue(":start", startDt);
    query.bindValue(":end", endDt);

    if (!query.exec()) {
        qWarning() << "[DataViewDialog] Query error:" << query.lastError().text();
        ui->statusLabel->setText("Load error");
        ui->progress->setVisible(false);
        return;
    }

    int rowCount = 0;
    while (query.next()) rowCount++;

    ui->progress->setMaximum(rowCount);
    ui->statusLabel->setText(QString("Loaded: %1 records").arg(rowCount));
    ui->progress->setVisible(false);
}

void DataViewDialog::updateGraph()
{
    ui->graph->clearGraphs();
    ui->graph->replot();
}

QString DataViewDialog::getSafeTableName(const QString &filePath)
{
    QString name = QFileInfo(filePath).fileName();
    name.replace(".", "_")
        .replace(" ", "_")
        .replace("-", "_")
        .replace("(", "_")
        .replace(")", "_")
        .replace("'", "_")
        .replace("\"", "_")
        .replace(",", "_")
        .replace(";", "_");
    name = name.toLower();
    return name;
}
