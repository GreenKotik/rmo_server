#include "dataviewdialog.h"
#include "ui_dataviewdialog.h"
#include "databasemanager.h"
#include "measurementsmodel.h"
#include "dataviewproxies.h"
#include "highlightrowdelegate.h"
#include "columnheaders.h"
#include "dbloaderworker.h"   // ✅ ДОБАВИТЬ
#include <QThread>            // ✅ ДОБАВИТЬ
#include <QJsonObject>        // ✅ ДОБАВИТЬ
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDateTime>
#include <QFileInfo>
#include <QDebug>
#include <QApplication>
#include <QTreeWidgetItemIterator>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFile>
#include <QDir>

// ============================================================================
// КОНСТРУКТОР
// ============================================================================
DataViewDialog::DataViewDialog(const QString &filePath, int docType, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DataViewDialog)
    , m_filePath(filePath)
    , m_docType(docType)
{
    ui->setupUi(this);
    m_dbConfig = loadDbConfigFromFile();
    setWindowTitle("Data View: " + QFileInfo(filePath).fileName());

    // По умолчанию — последние двое суток (вчера + сегодня)
    ui->dateStart->setDate(QDate::currentDate().addDays(-1));
    ui->dateEnd->setDate(QDate::currentDate());

    // Настройка UI
    ui->graph->setMouseTracking(true);

    setupGraph();
    setupTreeChannels();
    setupModels();
    setupConnections();

    // ✅ Подсветка «сегодня» в обоих календарях
    updateCalendarHighlights();
    // Автоматическая загрузка при открытии
    loadDataFromDB();
}

DataViewDialog::~DataViewDialog()
{
    // ✅ Останавливаем рабочий поток
    if (m_loaderThread && m_loaderThread->isRunning()) {
        if (m_loaderWorker) {
            m_loaderWorker->cancel();
        }
        m_loaderThread->quit();
        m_loaderThread->wait(3000);
    }

    // ✅ Гарантированно удаляем диалог прогресса
    destroyProgressDialog();

    delete ui;
}

void DataViewDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    if (ui->centerSplitter) {
        const int total = ui->centerSplitter->height();
        QList<int> sizes;
        sizes << qRound(total * 0.06);  // фильтр
        sizes << qRound(total * 0.20);  // журнал
        sizes << qRound(total * 0.25);  // таблица
        sizes << qRound(total * 0.49);  // график
        ui->centerSplitter->setSizes(sizes);
    }
}

// ============================================================================
// НАСТРОЙКА ГРАФИКА
// ============================================================================
void DataViewDialog::setupGraph()
{
    // ========================================================================
    // ✅ 1. Оформление фона графика в тон колонке "Дата_и_время"
    // ========================================================================
    const QColor timeColumnColor("#E8F0F8");

    // Фон самого виджета (область вокруг осей)
    ui->graph->setBackground(QBrush(timeColumnColor));

    // Фон области внутри осей (где рисуются сами графики)
    ui->graph->axisRect()->setBackground(QBrush(timeColumnColor));
    ui->graph->axisRect()->setBackgroundScaled(false);

    // ========================================================================
    // ✅ 2. Сетка темнее (чтобы читалась на голубоватом фоне)
    // ========================================================================
    const QColor gridColor("#7A828A");
    ui->graph->xAxis->grid()->setPen(QPen(gridColor, 1, Qt::DotLine));
    ui->graph->yAxis->grid()->setPen(QPen(gridColor, 1, Qt::DotLine));

    // Нулевая линия — сплошная, ещё темнее
    const QColor zeroLineColor("#00A800");
    ui->graph->xAxis->grid()->setZeroLinePen(QPen(zeroLineColor, 1, Qt::SolidLine));
    ui->graph->yAxis->grid()->setZeroLinePen(QPen(zeroLineColor, 1, Qt::SolidLine));
    // ========================================================================
    // Ось-дата и прочие настройки (как было)
    // ========================================================================
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat(m_currentTimeFormat);  // формат хранится в поле класса
    dateTicker->setTickCount(DETAILED_ROWS);   // разумная плотность меток
    ui->graph->xAxis->setTicker(dateTicker);
    ui->graph->xAxis->setTickLabelRotation(-90);
    ui->graph->xAxis->setTickLabelFont(QFont("Arial", 8));

//    ui->graph->xAxis->setLabel("Время");
//    ui->graph->yAxis->setLabel("Значение");
    ui->graph->xAxis->grid()->setVisible(true);
    ui->graph->yAxis->grid()->setVisible(true);

    // ========================================================================
    // ✅ Правая ось Y: зеркало левой с числами
    // ========================================================================
    QCPAxis *y2 = ui->graph->yAxis2;

    y2->setVisible(true);
    y2->setTickLabels(true);
    y2->setTicks(true);
    y2->setSubTicks(false);
    y2->grid()->setVisible(false);  // без двойной сетки

    // Оформление — как у левой оси
    y2->setTickLabelFont(ui->graph->yAxis->tickLabelFont());
    y2->setTickLabelColor(ui->graph->yAxis->tickLabelColor());
    y2->setBasePen(ui->graph->yAxis->basePen());
    y2->setTickPen(ui->graph->yAxis->tickPen());

    // Нулевая линия — тоже зелёная (для симметрии)
    y2->grid()->setZeroLinePen(QPen(zeroLineColor, 1, Qt::SolidLine));

    // Синхронизация диапазона yAxis → yAxis2
    connect(ui->graph->yAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            y2,
            QOverload<const QCPRange&>::of(&QCPAxis::setRange));

    // Сразу выставить текущий диапазон
    y2->setRange(ui->graph->yAxis->range());

    ui->graph->legend->setVisible(true);
    ui->graph->legend->setFont(QFont("Arial", 8));
    // ✅ Переносим легенду в левый верхний угол
    ui->graph->axisRect()->insetLayout()->setInsetAlignment(
        0,  // индекс легенды в inset layout (обычно 0)
        Qt::AlignLeft | Qt::AlignTop
    );

    // ✅ ЛКМ — drag/zoom, ПКМ — маркеры (не мешает навигации)
    ui->graph->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // ✅ Подключаем сигналы мыши
    connect(ui->graph, &QCustomPlot::mousePress,
            this, &DataViewDialog::onGraphMousePress);
    connect(ui->graph, &QCustomPlot::mouseMove,
            this, &DataViewDialog::onGraphMouseMove);
    connect(ui->graph, &QCustomPlot::mouseRelease,
            this, &DataViewDialog::onGraphMouseRelease);

    // ✅ Создаём три постоянных линии (синюю и две красные)
    // Они живут всё время существования диалога, скрываются/показываются.
    m_markerLine = new QCPItemLine(ui->graph);
    m_markerLine->setPen(QPen(Qt::blue, 2));
    m_markerLine->setVisible(false);

    m_rangeStartLine = new QCPItemLine(ui->graph);
    m_rangeStartLine->setPen(QPen(Qt::red, 2));
    m_rangeStartLine->setVisible(false);

    m_rangeEndLine = new QCPItemLine(ui->graph);
    m_rangeEndLine->setPen(QPen(Qt::red, 2));
    m_rangeEndLine->setVisible(false);

    // ✅ Полупрозрачная заливка диапазона (видна только во время протяжки)
    m_rangeRect = new QCPItemRect(ui->graph);
    m_rangeRect->setPen(QPen(QColor(0, 100, 255, 180), 1));  // синяя рамка
    m_rangeRect->setBrush(QBrush(QColor(0, 100, 255, 60)));  // полупрозрачная синяя заливка
    m_rangeRect->setVisible(false);

    // Перестраивать границы суток при изменении видимого диапазона
    connect(ui->graph->xAxis,
            QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
            this, [this](const QCPRange&) {
        rebuildDayBoundaryLines();
        updateTimeAxisFormat();      // ← новое
        ui->graph->replot();
    });
}

// ============================================================================
// ДЕРЕВО КАНАЛОВ
// ============================================================================
void DataViewDialog::setupTreeChannels()
{
    ui->treeChannels->clear();
    ui->treeChannels->setHeaderLabel("Показать данные");

    if (m_docType == 1) {
        // --- Антенна ---
        auto *base = new QTreeWidgetItem(ui->treeChannels, {"Базовые параметры"});
        base->setCheckState(0, Qt::Unchecked);
        const QStringList baseCols = {"Азимут", "Угол_места",
                                      "Угол_поляризации", "Уровень_сигнала"};
        for (const auto &c : baseCols) {
            auto *it = new QTreeWidgetItem(base, {c});
            it->setCheckState(0, Qt::Unchecked);
        }

        auto *temp = new QTreeWidgetItem(ui->treeChannels, {"Температура"});
        temp->setCheckState(0, Qt::Unchecked);
        for (int i = 1; i <= 8; ++i) {
            auto *t = new QTreeWidgetItem(temp, {QString("Temperatura%1").arg(i)});
            t->setCheckState(0, Qt::Unchecked);
        }

        auto *ibp = new QTreeWidgetItem(ui->treeChannels, {"ИБП-1"});
        ibp->setCheckState(0, Qt::Unchecked);
        for (const auto &c : {"IBP1_V", "IBP1_A1", "IBP1_W1"}) {
            auto *it = new QTreeWidgetItem(ibp, {c});
            it->setCheckState(0, Qt::Unchecked);
        }
    }
    else if (m_docType == 2) {
        // --- Передатчик ---
        auto *per1 = new QTreeWidgetItem(ui->treeChannels, {"Усилитель мощности \"A\""});
        per1->setCheckState(0, Qt::Unchecked);
        for (const auto &c : {"PerPowerIn1", "PerPower1", "PerPowerOtr1",
                              "Attenuator1", "Spiral1"}) {
            makeChannelItem(per1, c);
        }
        auto *per2 = new QTreeWidgetItem(ui->treeChannels, {"Усилитель мощности \"B\""});
        per2->setCheckState(0, Qt::Unchecked);
        for (const auto &c : {"PerPowerIn2", "PerPower2", "PerPowerOtr2",
                              "Attenuator2", "Spiral2"}) {
            makeChannelItem(per2, c);
        }
        auto *per3 = new QTreeWidgetItem(ui->treeChannels, {"Усилитель мощности \"C\""});
        per3->setCheckState(0, Qt::Unchecked);
        for (const auto &c : {"PerPowerIn3", "PerPower3", "PerPowerOtr3",
                              "Attenuator3", "Spiral3"}) {
            makeChannelItem(per3, c);
        }
        auto *temp = new QTreeWidgetItem(ui->treeChannels, {"Температура"});
        temp->setCheckState(0, Qt::Unchecked);
        for (const auto &c : {"Temper1", "Temper2"}) {
            makeChannelItem(temp, c);
        }
    }
    else if (m_docType == 3) {
        // --- Энерго ---
        for (int vv = 1; vv <= 4; ++vv) {
            auto *vvIn = new QTreeWidgetItem(ui->treeChannels,
                                             {QString("VV%1 Input").arg(vv)});
            vvIn->setCheckState(0, Qt::Unchecked);
            for (int i = 1; i <= 3; ++i) {
                auto *it = new QTreeWidgetItem(vvIn,
                    {QString("VV%1_In_V%2").arg(vv).arg(i)});
                it->setCheckState(0, Qt::Unchecked);
            }
        }
        for (int ibp = 1; ibp <= 3; ++ibp) {
            auto *ibpItem = new QTreeWidgetItem(ui->treeChannels,
                                                {QString("IBP-%1").arg(ibp)});
            ibpItem->setCheckState(0, Qt::Unchecked);
            for (const auto &c : {QString("IBP%1_Bat_V").arg(ibp),
                                  QString("IBP%1_Bat_A").arg(ibp),
                                  QString("IBP%1_Bat_Zarad").arg(ibp)}) {
                auto *it = new QTreeWidgetItem(ibpItem, {c});
                it->setCheckState(0, Qt::Unchecked);
            }
        }
    }
    else if (m_docType == 4) {
        // --- Модулятор ---
        auto *mod = new QTreeWidgetItem(ui->treeChannels, {"Модулятор"});
        mod->setCheckState(0, Qt::Unchecked);
        auto *st = new QTreeWidgetItem(mod, {"Статус"});
        st->setCheckState(0, Qt::Unchecked);
    }

    // Отключаем автотристейт (мы управляем им сами)
    QTreeWidgetItemIterator it(ui->treeChannels);
    while (*it) {
        (*it)->setFlags((*it)->flags() & ~Qt::ItemIsAutoTristate);
        ++it;
    }

    ui->treeChannels->expandAll();
    // ✅ Восстанавливаем состояние чеков из <doc>.tree
    restoreTreeState();
}

// ============================================================================
// МОДЕЛИ И ПРОКСИ
// ============================================================================
void DataViewDialog::setupModels()
{
    // Единая модель
    m_model = new MeasurementsModel(this);

    // Прокси для таблицы данных
    m_dataProxy = new DataTableProxy(this);
    m_dataProxy->setSourceModel(m_model);

    // Прокси для журнала
    m_journalProxy = new JournalProxy(this);
    m_journalProxy->setSourceModel(m_model);

    // Привязка к таблицам
    ui->tableData->setModel(m_dataProxy);
    ui->tableJournal->setModel(m_journalProxy);

    // ✅ Единый шрифт для обеих таблиц
    QFont tableFont = ui->tableData->font();
    tableFont.setPointSize(8);        // размер подберите под ваш UI
    ui->tableData->setFont(tableFont);
    ui->tableJournal->setFont(tableFont);

    // ✅ Делегат автовысоты для журнала аварий
    auto *journalHeightDelegate = new JournalHeightDelegate(
        m_journalProxy, m_model,
        200,
        ui->tableJournal);
    journalHeightDelegate->setSpecialColumnName("Дата и время");
    journalHeightDelegate->setSpecialColumnColor(QColor("#E8F0F8"));
    ui->tableJournal->setItemDelegate(journalHeightDelegate);

    // Настройки отображения
    ui->tableData->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableData->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableData->setAlternatingRowColors(true);
    ui->tableData->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->tableData->horizontalHeader()->setStretchLastSection(true);
    ui->tableData->setWordWrap(false);

    ui->tableJournal->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableJournal->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableJournal->setAlternatingRowColors(true);
    ui->tableJournal->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->tableJournal->horizontalHeader()->setStretchLastSection(true);
    ui->tableJournal->setWordWrap(false);

    // ✅ Делегат для подсветки текущей строки
    m_rowHighlighter = new HighlightRowDelegate(ui->tableData);
    m_rowHighlighter->setSpecialColumnName("Дата и время");
    m_rowHighlighter->setSpecialColumnColor(QColor("#E8F0F8"));  // голубовато-серый
    m_rowHighlighter->setProxyAndModel(m_dataProxy, m_model);   // ← ДОБАВИТЬ
    ui->tableData->setItemDelegate(m_rowHighlighter);

    // ========================================================================
    // Настройка высоты строк
    // ========================================================================
    const int rowHeight = 22;

    ui->tableData->verticalHeader()->setVisible(false);
//    ui->tableData->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableData->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableData->verticalHeader()->setMinimumSectionSize(rowHeight);
    ui->tableData->verticalHeader()->setDefaultSectionSize(rowHeight);

    ui->tableJournal->verticalHeader()->setVisible(false);
    // ✅ Автовысота строк по sizeHint делегата
    ui->tableJournal->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableJournal->verticalHeader()->setMinimumSectionSize(rowHeight);
    ui->tableJournal->verticalHeader()->setDefaultSectionSize(rowHeight);

    // ========================================================================
    // Стилизация заголовков и ячеек
    // ========================================================================
    const QString tableQss = R"(
        /* === Ячейки === */
        QTableView {
            background-color: #FFFFFF;
            gridline-color: #E0E0E0;
            selection-background-color: transparent;   /* подсветку делает делегат */
            selection-color: black;
        }
        QTableView::item {
            padding: 0px;
            margin: 0px;
            border: none;
        }

        /* === Горизонтальный заголовок (названия столбцов) === */
        QHeaderView::section:horizontal {
            background-color: #005A9E;
            color: #FFFFFF;
            padding: 4px 8px;
            border: none;
            border-right: 1px solid #004A8E;
            border-bottom: 1px solid #004A8E;
            font-weight: bold;
            font-size: 14px;
        }
        QHeaderView::section:horizontal:last {
            border-right: none;
        }
        QHeaderView::section:horizontal:hover {
            background-color: #0070C0;
        }

        /* === Вертикальный заголовок (скрыт, но на всякий случай) === */
        QHeaderView::section:vertical {
            background-color: #F0F0F0;
            border: none;
        }

        /* === Угловая кнопка (в левом верхнем углу) === */
        QTableView QTableCornerButton::section {
            background-color: #005A9E;
            border: none;
        }
    )";

    ui->tableData->setStyleSheet(tableQss);
    ui->tableJournal->setStyleSheet(tableQss);

    // ============================================================================
    // ✅ Разрешить перенос слов в заголовках таблиц
    // ============================================================================
    auto setupHeaderWrap = [](QTableView *table) {
        QHeaderView *h = table->horizontalHeader();
        // ✅ Включаем перенос слов через флаг выравнивания
        h->setDefaultAlignment(Qt::AlignCenter | (Qt::Alignment)Qt::TextWordWrap);

        // Подберите под свой шрифт: при 10pt примерно 40-44 px на 2 строки
        h->setMinimumHeight(42);
        h->setMaximumHeight(42);
    };

    setupHeaderWrap(ui->tableData);
    setupHeaderWrap(ui->tableJournal);
}

// ============================================================================
// ПОДКЛЮЧЕНИЕ СИГНАЛОВ
// ============================================================================
void DataViewDialog::setupConnections()
{
    connect(ui->btnLoad, &QPushButton::clicked,
            this, &DataViewDialog::onLoadDataClicked);
    connect(ui->btnExport, &QPushButton::clicked,
            this, &DataViewDialog::onExportClicked);
    connect(ui->treeChannels, &QTreeWidget::itemChanged,
            this, &DataViewDialog::onTreeItemChanged);

    connect(ui->dateStart, &QDateEdit::dateChanged,
            this, &DataViewDialog::onLoadDataClicked);
    connect(ui->dateEnd, &QDateEdit::dateChanged,
            this, &DataViewDialog::onLoadDataClicked);

    connect(ui->btnGraphMode, &QPushButton::clicked,
            this, &DataViewDialog::onGraphModeToggled);
    connect(ui->sliderGraph, &QSlider::valueChanged,
            this, &DataViewDialog::onGraphSliderMoved);

    // Синхронизация выделения: журнал → таблица данных
    connect(ui->tableJournal->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {


        if (!current.isValid()) return;
        const QModelIndex srcIdx = m_journalProxy->mapToSource(current);
        const QModelIndex dataIdx = m_dataProxy->mapFromSource(srcIdx);
        if (dataIdx.isValid()) {
            ui->tableData->selectionModel()->setCurrentIndex(
                dataIdx,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            ui->tableData->scrollTo(dataIdx, QAbstractItemView::PositionAtTop);
        }
    });

    // ✅ Синхронизация: таблица данных → журнал
    connect(ui->tableData->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {
        if (!current.isValid()) return;

        // ✅ Получаем row из source-индекса (не зависит от column)
        const QModelIndex srcIdx = m_dataProxy->mapToSource(current);
        if (!srcIdx.isValid()) return;
        const int row = srcIdx.row();

        // ✅ Строим индекс в журнале ПО СТОЛБЦУ timeColumnIndex,
        //    потому что другие столбцы отфильтрованы журналом.
        const QModelIndex journalSrcIdx =
            m_model->index(row, m_timeColumnIndex);
        const QModelIndex jIdx = m_journalProxy->mapFromSource(journalSrcIdx);

        if (jIdx.isValid()) {
            ui->tableJournal->selectionModel()->setCurrentIndex(
                jIdx,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            ui->tableJournal->scrollTo(jIdx, QAbstractItemView::PositionAtTop);
        }
    });

    // ✅ Таблица данных → синяя метка на графике
    connect(ui->tableData->selectionModel(),
            &QItemSelectionModel::currentChanged,   // ← тоже currentChanged
            this, [this](const QModelIndex &current, const QModelIndex &) {
        if (!current.isValid()) {
            m_markerRow = -1;
            QTimer::singleShot(0, this, &DataViewDialog::drawMarkers);
            return;
        }

        const QModelIndex srcIdx = m_dataProxy->mapToSource(current);
        if (!srcIdx.isValid()) return;

        const int row = srcIdx.row();
        if (row < 0 || row >= m_model->rowCount()) return;

        if (m_markerRow == row) return;   // ✅ защита от лишней работы

        m_markerRow = row;

        if (m_detailedMode) {
            const int totalRows = m_model->rowCount();
            const int maxStart = qMax(0, totalRows - DETAILED_ROWS);
            int newStart = row - DETAILED_ROWS / 2;
            newStart = qBound(0, newStart, maxStart);
            ui->sliderGraph->setValue(newStart);
            ui->tableData->scrollTo(current, QAbstractItemView::PositionAtTop);
        } else {
            QTimer::singleShot(0, this, &DataViewDialog::drawMarkers);
        }
    });

    // В setupModels() или setupConnections():
    connect(ui->tableData->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &DataViewDialog::onTableScrolled);
}

// ============================================================================
// ЗАГРУЗКА ДАННЫХ
// ============================================================================
void DataViewDialog::loadDataFromDB()
{
    startAsyncLoad();
}

// ============================================================================
// СИНХРОНИЗАЦИЯ СТОЛБЦОВ ТАБЛИЦЫ С ДЕРЕВОМ
// ============================================================================
void DataViewDialog::syncVisibleColumnsFromTree()
{
    QSet<int> visible;
    if (m_timeColumnIndex >= 0)
        visible.insert(m_timeColumnIndex);

    QTreeWidgetItemIterator it(ui->treeChannels);
    while (*it) {
        QTreeWidgetItem *item = *it;
        if (item->childCount() == 0 && item->checkState(0) == Qt::Checked) {
            const QString dbName = item->data(0, Qt::UserRole).toString();
            if (!dbName.isEmpty()) {
                const int idx = m_model->columnIndex(dbName);
                if (idx >= 0 && idx != m_errorColumnIndex)
                    visible.insert(idx);
            }
        }
        ++it;
    }

    // ✅ ErrorsAll — всегда видим (последним столбцом)
    if (m_errorColumnIndex >= 0)
        visible.insert(m_errorColumnIndex);

    m_dataProxy->setVisibleColumns(visible);

    applyColumnWidths();

    // ✅ Переместить столбец "ErrorsAll" в самый конец таблицы
    moveErrorsColumnToEnd();
}

// ============================================================================
// Перемещает столбец "ErrorsAll" в самый конец видимой таблицы.
// QTableView не поддерживает сортировку столбцов сама по себе, но у
// QHeaderView есть moveSection — он визуально переставляет столбцы.
// ============================================================================
void DataViewDialog::moveErrorsColumnToEnd()
{
    if (m_errorColumnIndex < 0) return;

    // Находим индекс столбца "ErrorsAll" в прокси
    const QModelIndex srcIdx = m_model->index(0, m_errorColumnIndex);
    const QModelIndex proxyIdx = m_dataProxy->mapFromSource(srcIdx);
    if (!proxyIdx.isValid()) return;

    const int from = proxyIdx.column();
    const int to = m_dataProxy->columnCount() - 1;

    if (from == to || from < 0) return;

    ui->tableData->horizontalHeader()->moveSection(from, to);
}

// ============================================================================
// ГРАФИК
// ============================================================================
void DataViewDialog::buildGraphs()
{
    ui->graph->clearGraphs();

    if (m_model->rowCount() == 0) {
        ui->graph->replot();
        return;
    }

    const QList<QString> checkedColumns = getCheckedColumns();
    if (checkedColumns.isEmpty()) {
        ui->graph->replot();
        return;
    }

    int startIdx = 0;
    int endIdx = m_model->rowCount();
    if (m_detailedMode) {
        startIdx = ui->sliderGraph->value();
        endIdx = qMin(startIdx + DETAILED_ROWS, m_model->rowCount());
    }

    const QVector<MeasurementsModel::Row> &rows = m_model->rows();

    for (const QString &colName : checkedColumns) {
        const int colIdx = m_model->columnIndex(colName);
        if (colIdx < 0) continue;

        QCPGraph *graph = ui->graph->addGraph();
        graph->setName(ColumnHeaders::displayName(colName));
        // ✅ Цвет фиксирован за каналом
        const QColor color = ColumnHeaders::color(colName);
        graph->setPen(QPen(color, 2));

//        graph->setLineStyle(QCPGraph::lsNone);
//        graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, color, color, 4));

        // pen всё равно нужен — он используется для легенды и выделения
        graph->setPen(QPen(color, 2));

        QVector<double> keys, values;
        keys.reserve(endIdx - startIdx);
        values.reserve(endIdx - startIdx);

        for (int i = startIdx; i < endIdx; ++i) {
            const auto &row = rows[i];
            if (!row.timestamp.isValid()) continue;
            const double key = row.timestamp.toMSecsSinceEpoch() / 1000.0;

            const QVariant v = row.values.value(colIdx);
            bool ok = false;
            double value = v.toDouble(&ok);
            if (!ok) {
                value = v.toString().toDouble(&ok);
                if (!ok) continue;
            }
            keys.append(key);
            values.append(value);
        }
        graph->setData(keys, values, true);
    }

    //  Y — жёстко по глобальному диапазону (не зависит от окна/зума)
    if (!m_yRangeValid) {
        recalcGlobalYRange();
    }
    if (m_yRangeValid) {
        ui->graph->yAxis->setRange(m_yMin, m_yMax);
    }
    //  X — по границам видимого окна.
    //  Полный режим: от первой до последней точки выборки.
    //  Детальный:    от startIdx до endIdx (DETAILED_ROWS точек).
    if (m_model->rowCount() > 0 && startIdx < endIdx) {
        const auto &rows = m_model->rows();
        const double xFirst =
            rows[startIdx].timestamp.toMSecsSinceEpoch() / 1000.0;
        const double xLast =
            rows[endIdx - 1].timestamp.toMSecsSinceEpoch() / 1000.0;
        // Небольшой запас по краям (2% от диапазона)
        const double span = xLast - xFirst;
        const double pad = (span > 0.0) ? span * 0.02 : 60.0;
        ui->graph->xAxis->setRange(xFirst - pad, xLast + pad);
    }
    ui->graph->replot();

    // ✅ Восстанавливаем видимость меток после перестройки графика
    QTimer::singleShot(0, this, &DataViewDialog::drawMarkers);
    QTimer::singleShot(0, this, &DataViewDialog::rebuildDayBoundaryLines);  // ← добавить
    QTimer::singleShot(0, this, &DataViewDialog::updateTimeAxisFormat);   // ← новое
}

void DataViewDialog::updateGraph()
{
    syncVisibleColumnsFromTree();
    buildGraphs();
}

QList<QString> DataViewDialog::getCheckedColumns() const
{
    QList<QString> result;
    QTreeWidgetItemIterator it(ui->treeChannels);
    while (*it) {
        QTreeWidgetItem *item = *it;
        if (item->childCount() == 0 && item->checkState(0) == Qt::Checked) {
            // ✅ Возвращаем имя в БД (UserRole)
            const QString dbName = item->data(0, Qt::UserRole).toString();
            if (!dbName.isEmpty())
                result << dbName;
        }
        ++it;
    }
    return result;
}

// ============================================================================
// ПЕРЕКЛЮЧЕНИЕ РЕЖИМА ГРАФИКА
// ============================================================================
void DataViewDialog::onGraphModeToggled()
{
    m_detailedMode = !m_detailedMode;

    if (m_detailedMode) {
        const int totalRows = m_model->rowCount();
        const int maxStart = qMax(0, totalRows - DETAILED_ROWS);
        ui->sliderGraph->setRange(0, maxStart);
        ui->sliderGraph->setValue(0);
        ui->sliderGraph->setVisible(true);
        ui->btnGraphMode->setText("  Показать полный график  ");
    } else {
        ui->sliderGraph->setVisible(false);
        ui->btnGraphMode->setText(" Показать график детально ");
    }

    buildGraphs();
}

void DataViewDialog::onGraphSliderMoved(int value)
{
    if (!m_detailedMode) return;

    // ✅ График → таблица: прокручиваем таблицу к первой видимой строке
    if (!m_syncingScroll) {
        m_syncingScroll = true;

        const int targetRow = value;   // первая видимая строка графика
        if (targetRow >= 0 && targetRow < m_dataProxy->rowCount()) {
            const QModelIndex idx = m_dataProxy->index(targetRow, 0);
            if (idx.isValid()) {
                ui->tableData->scrollTo(idx, QAbstractItemView::PositionAtTop);
            }
        }

        m_syncingScroll = false;
    }

    buildGraphs();
}

// ============================================================================
// МЕТКИ И ДИАПАЗОН НА ГРАФИКЕ
// ЛКМ одиночный клик → синяя метка
// ПКМ 1-й клик       → первая красная метка (начало диапазона)
// ПКМ 2-й клик       → вторая красная метка (конец) + перезагрузка данных
// ПКМ 3-й клик       → сброс и начало заново
// Ctrl + ПКМ         → сброс всего
// ============================================================================
void DataViewDialog::onGraphMousePress(QMouseEvent *event)
{
    // -------------------------------------------------------------------
    // ПРАВАЯ КНОПКА — начало протяжки диапазона
    // -------------------------------------------------------------------
    if (event->button() == Qt::RightButton) {
        // Ctrl + ПКМ → сброс всего
        if (event->modifiers() & Qt::ControlModifier) {
            m_markerRow = -1;
            m_rangeStartRow = -1;
            m_rangeEndRow = -1;
            m_rangingRange = false;
            QTimer::singleShot(0, this, &DataViewDialog::drawMarkers);
            return;
        }

        const int idx = rowIndexFromPixelX(event->pos().x());
        if (idx < 0) return;

        // ✅ Начало протяжки: первая метка = текущая точка
        m_rangeStartRow = idx;
        m_rangeEndRow = idx;
        m_rangingRange = true;

        qDebug() << "[RightPress] start row =" << idx;

        QTimer::singleShot(0, this, &DataViewDialog::drawMarkers);
        return;
    }

    // -------------------------------------------------------------------
    // ЛЕВАЯ КНОПКА — только навигация или одиночный клик для синей метки
    // -------------------------------------------------------------------
    if (event->button() != Qt::LeftButton) return;

    m_pressPos = event->pos();
    m_selecting = false;
}

void DataViewDialog::onGraphMouseMove(QMouseEvent *event)
{
    // Отображение координат под курсором
    if (m_model->rowCount() > 0) {
        const double key = ui->graph->xAxis->pixelToCoord(event->pos().x());
        const double value = ui->graph->yAxis->pixelToCoord(event->pos().y());

        if (key >= ui->graph->xAxis->range().lower &&
            key <= ui->graph->xAxis->range().upper &&
            value >= ui->graph->yAxis->range().lower &&
            value <= ui->graph->yAxis->range().upper)
        {
            const QDateTime dt = QDateTime::fromMSecsSinceEpoch(qint64(key * 1000.0));
            ui->labelCoordinates->setText(
                QString("Время: %1   Значение: %2")
                    .arg(dt.toString("dd.MM.yyyy hh:mm:ss"))
                    .arg(value, 0, 'f', 2));
        } else {
            ui->labelCoordinates->setText("Время: --   Значение: --");
        }
    }

    // -------------------------------------------------------------------
    // ПРОТЯЖКА ПКМ: обновляем конец диапазона
    // -------------------------------------------------------------------
    if (m_rangingRange && (event->buttons() & Qt::RightButton)) {
        const int idx = rowIndexFromPixelX(event->pos().x());
        if (idx >= 0 && idx != m_rangeEndRow) {
            m_rangeEndRow = idx;
            QTimer::singleShot(0, this, &DataViewDialog::drawMarkers);
        }
        return;  // не доходим до логики ЛКМ
    }

    // -------------------------------------------------------------------
    // ЛКМ: отслеживаем протяжку (только для определения клик/drag)
    // -------------------------------------------------------------------
    if (!(event->buttons() & Qt::LeftButton)) return;

    if (!m_selecting &&
        (event->pos() - m_pressPos).manhattanLength() >= QApplication::startDragDistance())
    {
        m_selecting = true;
    }
}

void DataViewDialog::onGraphMouseRelease(QMouseEvent *event)
{
    // -------------------------------------------------------------------
    // ПРАВАЯ КНОПКА — финализация диапазона
    // -------------------------------------------------------------------
    if (event->button() == Qt::RightButton) {
        if (!m_rangingRange) return;

        m_rangingRange = false;

        // Обновляем конечную точку на момент отпускания
        const int idx = rowIndexFromPixelX(event->pos().x());
        if (idx >= 0) m_rangeEndRow = idx;

        if (m_rangeStartRow < 0 || m_rangeEndRow < 0) {
            // Нечего финализировать
            QTimer::singleShot(0, this, &DataViewDialog::drawMarkers);
            return;
        }

        // Сортируем
        if (m_rangeStartRow > m_rangeEndRow)
            std::swap(m_rangeStartRow, m_rangeEndRow);

        // Если диапазон пустой (одна точка) — просто оставляем маркер
        if (m_rangeStartRow == m_rangeEndRow) {
            qDebug() << "[RightRelease] одиночная точка =" << m_rangeStartRow;
            QTimer::singleShot(0, this, &DataViewDialog::drawMarkers);
            return;
        }

        const QDateTime startDt = m_model->rows()[m_rangeStartRow].timestamp;
        const QDateTime endDt   = m_model->rows()[m_rangeEndRow].timestamp;

        qDebug() << "[RightRelease] range rows:" << m_rangeStartRow
                 << "—" << m_rangeEndRow
                 << " time:" << startDt << "—" << endDt;

        // ✅ Запоминаем точное время внутри суток
        m_timeStart = startDt.time();
        m_timeEnd   = endDt.time();

        // Обновляем фильтр дат
        ui->dateStart->blockSignals(true);
        ui->dateEnd->blockSignals(true);
        ui->dateStart->setDate(startDt.date());
        ui->dateEnd->setDate(endDt.date());
        ui->dateStart->blockSignals(false);
        ui->dateEnd->blockSignals(false);

        // Перезагружаем данные за диапазон
        QTimer::singleShot(0, this, &DataViewDialog::loadDataFromDB);
        return;
    }

    // -------------------------------------------------------------------
    // ЛЕВАЯ КНОПКА — одиночный клик ставит синюю метку
    // -------------------------------------------------------------------
    if (event->button() != Qt::LeftButton) return;

    if (m_selecting) {
        m_selecting = false;
        return;
    }

    const int idx = rowIndexFromPixelX(event->pos().x());
    if (idx < 0) return;

    m_markerRow = idx;  // синяя метка, m_range* не трогаем

    const QDateTime dt = m_model->rows()[idx].timestamp;
    qDebug() << "[LeftClick] marker row =" << idx << "time:" << dt;

    QTimer::singleShot(0, this, [this, idx]() {
        const QModelIndex srcIdx = m_model->index(idx, m_timeColumnIndex);
        const QModelIndex dataIdx = m_dataProxy->mapFromSource(srcIdx);
        if (dataIdx.isValid()) {
            ui->tableData->selectionModel()->setCurrentIndex(
                dataIdx,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            ui->tableData->scrollTo(dataIdx, QAbstractItemView::PositionAtTop);
        }
    });

    QTimer::singleShot(0, this, &DataViewDialog::drawMarkers);
}

// ============================================================================
// ОТРИСОВКА МЕТОК (изменяем координаты существующих линий)
// ============================================================================
void DataViewDialog::drawMarkers()
{
    if (!m_markerLine || !m_rangeStartLine || !m_rangeEndLine || !m_rangeRect) return;

    if (m_model->rowCount() == 0) {
        m_markerLine->setVisible(false);
        m_rangeStartLine->setVisible(false);
        m_rangeEndLine->setVisible(false);
        m_rangeRect->setVisible(false);
        ui->graph->replot();
        return;
    }

    const auto &rows = m_model->rows();
    const double yLow = ui->graph->yAxis->range().lower;
    const double yHigh = ui->graph->yAxis->range().upper;

    // Синяя метка (ЛКМ)
    if (m_markerRow >= 0 && m_markerRow < rows.size()
        && rows[m_markerRow].timestamp.isValid())
    {
        const double x = rows[m_markerRow].timestamp.toMSecsSinceEpoch() / 1000.0;
        m_markerLine->start->setCoords(x, yLow);
        m_markerLine->end->setCoords(x, yHigh);
        m_markerLine->setVisible(true);
    } else {
        m_markerLine->setVisible(false);
    }

    // Первая красная метка (начало диапазона)
    if (m_rangeStartRow >= 0 && m_rangeStartRow < rows.size()
        && rows[m_rangeStartRow].timestamp.isValid())
    {
        const double x = rows[m_rangeStartRow].timestamp.toMSecsSinceEpoch() / 1000.0;
        m_rangeStartLine->start->setCoords(x, yLow);
        m_rangeStartLine->end->setCoords(x, yHigh);
        m_rangeStartLine->setVisible(true);
    } else {
        m_rangeStartLine->setVisible(false);
    }

    // Вторая красная метка (конец диапазона)
    if (m_rangeEndRow >= 0 && m_rangeEndRow < rows.size()
        && rows[m_rangeEndRow].timestamp.isValid())
    {
        const double x = rows[m_rangeEndRow].timestamp.toMSecsSinceEpoch() / 1000.0;
        m_rangeEndLine->start->setCoords(x, yLow);
        m_rangeEndLine->end->setCoords(x, yHigh);
        m_rangeEndLine->setVisible(true);
    } else {
        m_rangeEndLine->setVisible(false);
    }

    // ✅ Полупрозрачная заливка диапазона — только во время протяжки ПКМ
    if (m_rangingRange
        && m_rangeStartRow >= 0 && m_rangeStartRow < rows.size()
        && m_rangeEndRow >= 0 && m_rangeEndRow < rows.size()
        && m_rangeStartRow != m_rangeEndRow)
    {
        const double x1 = rows[m_rangeStartRow].timestamp.toMSecsSinceEpoch() / 1000.0;
        const double x2 = rows[m_rangeEndRow].timestamp.toMSecsSinceEpoch() / 1000.0;
        const double xLeft  = qMin(x1, x2);
        const double xRight = qMax(x1, x2);

        m_rangeRect->topLeft->setCoords(xLeft, yHigh);       // верх-лево
        m_rangeRect->bottomRight->setCoords(xRight, yLow);   // низ-право
        m_rangeRect->setVisible(true);
    } else {
        m_rangeRect->setVisible(false);
    }

    ui->graph->replot();
}


void DataViewDialog::resetMarkers(bool doReplot)
{
    m_markerRow = -1;
    m_rangeStartRow = -1;
    m_rangeEndRow = -1;
    m_rangingRange = false;

    if (m_markerLine)     m_markerLine->setVisible(false);
    if (m_rangeStartLine) m_rangeStartLine->setVisible(false);
    if (m_rangeEndLine)   m_rangeEndLine->setVisible(false);
    if (m_rangeRect)      m_rangeRect->setVisible(false);

    if (ui->tableData->selectionModel())
        ui->tableData->selectionModel()->clearSelection();
    if (ui->tableJournal->selectionModel())
        ui->tableJournal->selectionModel()->clearSelection();

    if (doReplot) {
        ui->graph->replot();
    }
}

// ============================================================================
// ПРОЧЕЕ
// ============================================================================
void DataViewDialog::onLoadDataClicked()
{
    // ✅ Ручная загрузка — сбрасываем точное время, берём полные сутки
    m_timeStart = QTime();
    m_timeEnd   = QTime();

    ui->statusLabel->setText("Загрузка...");
    loadDataFromDB();
}

void DataViewDialog::onExportClicked()
{
    ui->statusLabel->setText("Экспорт...");
    // TODO: следующая итерация
}

void DataViewDialog::onTreeItemChanged(QTreeWidgetItem *item, int /*column*/)
{
    // Защита от рекурсии: если мы сами программно меняем чеки — ничего не делаем
    if (m_updatingTree || !item) return;

    m_updatingTree = true;

    // 1. Если у элемента есть дети — это родитель.
    //    Ставим/снимаем все дочерние чеки рекурсивно по состоянию родителя.
    if (item->childCount() > 0) {
        const Qt::CheckState state = item->checkState(0);
        // Если узел стал PartiallyChecked (после setCheckState вручную) —
        // трактуем как Checked для каскада вниз (стандартное поведение MFC).
        const Qt::CheckState cascade =
            (state == Qt::PartiallyChecked) ? Qt::Checked : state;
        applyCheckStateToChildren(item, cascade);
    }

    // 2. Обновляем состояние родителя (если он есть).
    //    Ребёнок изменился → родитель может стать Checked/Unchecked/PartiallyChecked.
    QTreeWidgetItem *parent = item->parent();
    while (parent) {
        updateParentCheckState(parent);
        parent = parent->parent();
    }

    m_updatingTree = false;
    // ✅ Сменился набор каналов — Y-диапазон пересчитать заново
    m_yRangeValid = false;

    // 3. Перестраиваем график под новый набор видимых столбцов
    updateGraph();
}

// ============================================================================
// Рекурсивно применяет состояние чеков ко всем дочерним элементам
// ============================================================================
void DataViewDialog::applyCheckStateToChildren(QTreeWidgetItem *parent, Qt::CheckState state)
{
    if (!parent) return;

    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        child->setCheckState(0, state);

        // Если у ребёнка тоже есть дети — идём глубже
        if (child->childCount() > 0) {
            applyCheckStateToChildren(child, state);
        }
    }
}

// ============================================================================
// Обновляет состояние родителя на основе чеков его детей.
// Логика:
//   - все Checked          → Checked
//   - все Unchecked        → Unchecked
//   - иначе                → PartiallyChecked
// (Поскольку мы отключили Qt::ItemIsAutoTristate, считаем сами.)
// ============================================================================
void DataViewDialog::updateParentCheckState(QTreeWidgetItem *parent)
{
    if (!parent || parent->childCount() == 0) return;

    int checkedCount = 0;
    int uncheckedCount = 0;
    const int total = parent->childCount();

    for (int i = 0; i < total; ++i) {
        const Qt::CheckState st = parent->child(i)->checkState(0);
        if (st == Qt::Checked) {
            ++checkedCount;
        } else if (st == Qt::Unchecked) {
            ++uncheckedCount;
        }
        // PartiallyChecked не учитываем ни в checked, ни в unchecked
    }

    Qt::CheckState newState;
    if (checkedCount == total) {
        newState = Qt::Checked;
    } else if (uncheckedCount == total) {
        newState = Qt::Unchecked;
    } else {
        newState = Qt::PartiallyChecked;
    }

    // setCheckState сам не эмитит itemChanged, если m_updatingTree == true
    // (мы находимся внутри этого блока), но подстрахуемся:
    if (parent->checkState(0) != newState) {
        parent->setCheckState(0, newState);
    }
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
    return name.toLower();
}

// ============================================================================
// ПОИСК БЛИЖАЙШЕЙ СТРОКИ ПО X-КООРДИНАТЕ
// ============================================================================
int DataViewDialog::rowIndexFromPixelX(int pixelX) const
{
    if (m_model->rowCount() == 0) return -1;

    const double key = ui->graph->xAxis->pixelToCoord(pixelX);

    // Определяем рабочий диапазон (в детальном режиме — окно)
    int startIdx = 0;
    int endIdx = m_model->rowCount();
    if (m_detailedMode) {
        startIdx = ui->sliderGraph->value();
        endIdx = qMin(startIdx + DETAILED_ROWS, m_model->rowCount());
    }

    const QVector<MeasurementsModel::Row> &rows = m_model->rows();

    // Линейный поиск ближайшей точки
    int bestIdx = -1;
    double bestDist = std::numeric_limits<double>::max();

    for (int i = startIdx; i < endIdx; ++i) {
        if (!rows[i].timestamp.isValid()) continue;
        const double t = rows[i].timestamp.toMSecsSinceEpoch() / 1000.0;
        const double dist = qAbs(t - key);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }

    return bestIdx;
}

void DataViewDialog::onTableScrolled(int /*value*/)
{
    if (!m_detailedMode) return;
    if (m_syncingScroll) return;

    m_syncingScroll = true;

    // ✅ Таблица → график: первая видимая строка
    // Берём верхнюю границу вертикального скролла
    int topRow = ui->tableData->rowAt(0);   // индекс модели под y=0
    if (topRow < 0) {
        // Если rowAt(0) не сработал (например, таблица пуста),
        // используем значение скроллбара напрямую
        topRow = ui->tableData->verticalScrollBar()->value();
    }

    if (topRow >= 0 && topRow < m_model->rowCount()) {
        // Устанавливаем слайдер (это вызовет onGraphSliderMoved,
        // но там m_syncingScroll == true, поэтому прокрутка таблицы не сработает)
        ui->sliderGraph->blockSignals(true);
        ui->sliderGraph->setValue(topRow);
        ui->sliderGraph->blockSignals(false);

        // Вручную перестраиваем график, потому что сигнал заблокирован
        buildGraphs();
    }

    m_syncingScroll = false;
}

// ============================================================================
// Путь к файлу состояния: <полный путь документа> + ".tree"
// Пример: "/data/Лидер-7.per" → "/data/Лидер-7.per.tree"
// ============================================================================
QString DataViewDialog::treeStateFilePath() const
{
    if (m_filePath.isEmpty()) return QString();
    return m_filePath + ".tree";
}

// ============================================================================
// Сохранение состояния чеков в <doc>.tree
// ============================================================================
void DataViewDialog::saveTreeState()
{
    if (m_filePath.isEmpty()) return;

    // Собираем все элементы в объект "channels": { "путь": состояние }
    QJsonObject channels;
    QJsonObject expanded;

    std::function<void(QTreeWidgetItem*, const QString &)> walk =
        [&](QTreeWidgetItem *item, const QString &prefix)
    {
        if (!item) return;

        const QString path = prefix.isEmpty()
            ? item->text(0)
            : prefix + "/" + item->text(0);

        channels[path] = static_cast<int>(item->checkState(0));

        // ✅ Состояние раскрытия — только для узлов с детьми
        if (item->childCount() > 0) {
            expanded[path] = item->isExpanded();
        }

        for (int i = 0; i < item->childCount(); ++i) {
            walk(item->child(i), path);
        }
    };

    for (int i = 0; i < ui->treeChannels->topLevelItemCount(); ++i) {
        walk(ui->treeChannels->topLevelItem(i), QString());
    }

    // Верхний уровень
    QJsonObject root;
    root["version"] = 1;
    root["docPath"] = m_filePath;
    root["channels"] = channels;
    root["expanded"] = expanded;    // ← новое

    // Записываем в <doc>.tree
    const QString statePath = treeStateFilePath();
    QFile f(statePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[DataViewDialog] Не удалось сохранить состояние дерева:"
                   << statePath << f.errorString();
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();

    qDebug() << "[DataViewDialog] Состояние дерева сохранено:" << statePath;
}

// ============================================================================
// Восстановление состояния чеков из <doc>.tree
// ============================================================================
void DataViewDialog::restoreTreeState()
{
    if (m_filePath.isEmpty()) return;

    const QString statePath = treeStateFilePath();
    QFile f(statePath);
    if (!f.exists()) {
        // Файла нет — оставляем дефолт (всё Unchecked)
        return;
    }

    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[DataViewDialog] Не удалось открыть файл состояния дерева:"
                   << statePath << f.errorString();
        return;
    }

    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[DataViewDialog] Ошибка парсинга .tree файла:"
                   << err.errorString() << "в" << statePath;
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonObject channels = root.value("channels").toObject();
    const QJsonObject expanded = root.value("expanded").toObject();  // ← новое
    if (channels.isEmpty()) return;

    // Раскладываем "path" → CheckState
    QHash<QString, Qt::CheckState> stateMap;
    for (auto it = channels.constBegin(); it != channels.constEnd(); ++it) {
        const int v = it.value().toInt();
        Qt::CheckState st = Qt::Unchecked;
        if (v == Qt::Checked)               st = Qt::Checked;
        else if (v == Qt::PartiallyChecked) st = Qt::PartiallyChecked;
        else                                st = Qt::Unchecked;
        stateMap.insert(it.key(), st);
    }

    // Раскладываем "path" → isExpanded
    QHash<QString, bool> expandedMap;
    for (auto it = expanded.constBegin(); it != expanded.constEnd(); ++it) {
        expandedMap.insert(it.key(), it.value().toBool());
    }

    // Применяем к дереву. Всё — под защитным флагом, чтобы не дёргать
    // onTreeItemChanged → updateGraph() на каждый setCheckState.
    m_updatingTree = true;

    std::function<void(QTreeWidgetItem*, const QString &)> walk =
        [&](QTreeWidgetItem *item, const QString &prefix)
    {
        if (!item) return;

        const QString path = prefix.isEmpty()
            ? item->text(0)
            : prefix + "/" + item->text(0);

        if (stateMap.contains(path)) {
            item->setCheckState(0, stateMap.value(path));
        }

        // ✅ Восстанавливаем раскрытие (если узел был в файле)
        if (item->childCount() > 0 && expandedMap.contains(path)) {
            item->setExpanded(expandedMap.value(path));
        }

        for (int i = 0; i < item->childCount(); ++i) {
            walk(item->child(i), path);
        }
    };

    for (int i = 0; i < ui->treeChannels->topLevelItemCount(); ++i) {
        walk(ui->treeChannels->topLevelItem(i), QString());
    }

    m_updatingTree = false;

    qDebug() << "[DataViewDialog] Состояние дерева восстановлено из:" << statePath;
}

void DataViewDialog::closeEvent(QCloseEvent *event)
{
    // ✅ Сообщаем, что загрузки больше нет — чтобы processEvents
    //    в updateProgressAndProcess не пытался обращаться к this.
    m_loadingActive = false;

    if (m_loaderThread && m_loaderThread->isRunning()) {
        if (m_loaderWorker) m_loaderWorker->cancel();
        m_loaderThread->quit();
        m_loaderThread->wait(2000);
    }

    // ✅ Прогресс-диалог скрываем (не удаляем — он переиспользуемый)
    closeProgressDialog();

    saveTreeState();
    QDialog::closeEvent(event);
}

// ============================================================================
// Рисует вертикальные зелёные пунктирные линии на границах суток,
// попадающих в текущий видимый диапазон X-оси.
// Пересоздаёт линии графика каждый раз.
// ============================================================================
void DataViewDialog::rebuildDayBoundaryLines()
{
    // 1. Удаляем старые линии
    for (QCPItemLine *line : m_dayBoundaryLines) {
        ui->graph->removeItem(line);
//       delete line;
    }
    m_dayBoundaryLines.clear();

    if (m_model->rowCount() == 0) return;

    // 2. Получаем видимый диапазон X
    const QCPRange xRange = ui->graph->xAxis->range();
    if (xRange.size() <= 0) return;

    // 3. Определяем диапазон дат
    const QDateTime dtStart = QDateTime::fromMSecsSinceEpoch(
        qint64(xRange.lower * 1000.0));
    const QDateTime dtEnd = QDateTime::fromMSecsSinceEpoch(
        qint64(xRange.upper * 1000.0));

    // 4. Первая полночь: если dtStart уже ровно 00:00:00 — берём её,
    //    иначе — следующие сутки.
    QDate day = dtStart.date();
    QDateTime firstMidnight(day, QTime(0, 0, 0));

    // Если первая полночь < dtStart — сдвигаем на следующие сутки
    if (firstMidnight < dtStart) {
        day = day.addDays(1);
        firstMidnight = QDateTime(day, QTime(0, 0, 0));
    }

    // 5. Идём по суткам и рисуем линию на каждой полуночи,
    //    пока не выйдем за правую границу видимого диапазона
    const QDateTime dtLimit = dtEnd.addDays(1);  // небольшой запас
    int guard = 0;
    const int MAX_LINES = 100; // защита от бесконечного цикла

    while (firstMidnight <= dtLimit && guard++ < MAX_LINES) {
        const double x = firstMidnight.toMSecsSinceEpoch() / 1000.0;

        QCPItemLine *line = new QCPItemLine(ui->graph);

        QPen pen(QColor(0, 128, 0), 1, Qt::DashLine);
        line->setPen(pen);
        line->setLayer("overlay");
        line->setSelectable(false);

        // X — в координатах оси X (время), Y — в долях axis rect (0..1)
        line->start->setTypeX(QCPItemPosition::ptPlotCoords);
        line->start->setTypeY(QCPItemPosition::ptAxisRectRatio);
        line->end->setTypeX(QCPItemPosition::ptPlotCoords);
        line->end->setTypeY(QCPItemPosition::ptAxisRectRatio);

        line->start->setCoords(x, 0.0);  // низ axis rect
        line->end->setCoords(x, 1.0);    // верх axis rect

        m_dayBoundaryLines.append(line);

        firstMidnight = firstMidnight.addDays(1);
    }
}

// ============================================================================
// Адаптивный формат оси времени:
//   ≤ 1 сутки  → "dd.MM hh:mm:ss"
//   > 1 сутки  → "dd.MM hh:mm"
// Вызывается при изменении видимого диапазона X.
// ============================================================================
void DataViewDialog::updateTimeAxisFormat()
{
    auto ticker = qSharedPointerDynamicCast<QCPAxisTickerDateTime>(
        ui->graph->xAxis->ticker());
    if (!ticker) return;

    const double size = ui->graph->xAxis->range().size();
    const double oneDay = 24.0 * 3600.0;

    const QString newFormat = (size < oneDay)
        ? QStringLiteral("dd.MM hh:mm:ss")
        : QStringLiteral("dd.MM hh:mm");

    if (m_currentTimeFormat == newFormat) return;

    m_currentTimeFormat = newFormat;
    ticker->setDateTimeFormat(newFormat);
}

// ============================================================================
// ✅ Пересчитывает глобальный Y-диапазон по отмеченным каналам,
//    используя уже готовые min/max из m_columnRanges (рассчитаны в воркере).
//    Работает за O(K), где K — количество отмеченных столбцов.
// ============================================================================
void DataViewDialog::recalcGlobalYRange()
{
    m_yRangeValid = false;
    m_yMin = 0.0;
    m_yMax = 1.0;

    if (!m_model || m_model->rowCount() == 0) return;

    const QList<QString> checkedColumns = getCheckedColumns();
    if (checkedColumns.isEmpty()) return;

    // Соберём индексы столбцов, по которым считаем min/max
    QList<int> colIndices;
    for (const QString &name : checkedColumns) {
        const int idx = m_model->columnIndex(name);
        if (idx >= 0 && idx < m_columnRanges.size()) {
            colIndices.append(idx);
        }
    }
    if (colIndices.isEmpty()) return;

    // ✅ Используем готовые min/max из m_columnRanges
    double mn = std::numeric_limits<double>::max();
    double mx = std::numeric_limits<double>::lowest();
    bool any = false;

    for (int colIdx : colIndices) {
        const QPair<double, double> &range = m_columnRanges[colIdx];

        // Маркер «нет данных»: first > second
        if (range.first > range.second) continue;

        if (range.first < mn)  mn = range.first;
        if (range.second > mx) mx = range.second;
        any = true;
    }

    if (!any) return;

    // Небольшой запас сверху/снизу, чтобы график не прилипал к краям
    const double span = mx - mn;
    const double pad = (span > 0.0) ? span * 0.05 : 1.0;
    m_yMin = mn - pad;
    m_yMax = mx + pad;
    m_yRangeValid = true;
}

// ============================================================================
// Устанавливает начальные ширины столбцов в обеих таблицах (таблица данных + журнал)
// ============================================================================
void DataViewDialog::applyColumnWidths()
{
    // Таблица данных: столбцы соответствуют видимым столбцам прокси
    // (прокси меняет порядок и индексы, поэтому сопоставляем через sourceModel).
    QHeaderView *hData = ui->tableData->horizontalHeader();
    for (int proxyCol = 0; proxyCol < m_dataProxy->columnCount(); ++proxyCol) {
        const QModelIndex proxyIdx = m_dataProxy->index(0, proxyCol);
        const QModelIndex srcIdx = m_dataProxy->mapToSource(proxyIdx);
        if (!srcIdx.isValid()) continue;

        const QString dbName = m_model->columnNames().value(srcIdx.column());
        const int w = ColumnHeaders::width(dbName);
        if (w > 0) {
            ui->tableData->setColumnWidth(proxyCol, w);
        }
    }

    // Журнал: обычно всего 2 столбца (Дата_и_время + ErrorsAll)
    QHeaderView *hJournal = ui->tableJournal->horizontalHeader();
    for (int proxyCol = 0; proxyCol < m_journalProxy->columnCount(); ++proxyCol) {
        const QModelIndex proxyIdx = m_journalProxy->index(0, proxyCol);
        const QModelIndex srcIdx = m_journalProxy->mapToSource(proxyIdx);
        if (!srcIdx.isValid()) continue;

        const QString dbName = m_model->columnNames().value(srcIdx.column());
        const int w = ColumnHeaders::width(dbName);
        if (w > 0) {
            ui->tableJournal->setColumnWidth(proxyCol, w);
        }
    }

    // В журнале последний столбец растягиваем
    if (ui->tableJournal->horizontalHeader()->count() > 0) {
        const int last = ui->tableJournal->horizontalHeader()->count() - 1;
        ui->tableJournal->horizontalHeader()->setSectionResizeMode(
            last, QHeaderView::Stretch);
    }
}

// ============================================================================
// Создаёт leaf-элемент дерева каналов.
// Отображает имя по русски, а имя в БД хранит в Qt::UserRole.
// ============================================================================
QTreeWidgetItem* DataViewDialog::makeChannelItem(QTreeWidgetItem *parent,
                                                 const QString &dbName)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(parent,
                                                {ColumnHeaders::displayName(dbName)});
    item->setData(0, Qt::UserRole, dbName);
    item->setCheckState(0, Qt::Unchecked);
    return item;
}

// ============================================================================
// Пересчитывает высоту строк таблицы данных ТОЛЬКО для аварийных строк
// (у которых количество аварий > 1). Остальные строки остаются стандартной
// высоты (defaultSectionSize). Это гораздо быстрее, чем ResizeToContents,
// который опрашивает sizeHint каждой ячейки.
// ============================================================================
void DataViewDialog::recalcRowHeightsTableData()
{
    if (!m_dataProxy || !m_model) return;

    const int lineHeight = ui->tableData->fontMetrics().height() + 2;
    const int maxHeight = 200;
    const int baseHeight = ui->tableData->verticalHeader()->defaultSectionSize();

    // ✅ 1. Сбрасываем ТОЛЬКО те строки, которые были расширены в прошлый раз
    for (int row : m_expandedRows) {
        if (row >= 0 && row < ui->tableData->model()->rowCount()) {
            ui->tableData->setRowHeight(row, baseHeight);
        }
    }
    m_expandedRows.clear();

    // ✅ 2. Устанавливаем высоту ТОЛЬКО аварийным строкам
    const int n = qMin(m_dataProxy->rowCount(), m_model->rowCount());
    if (m_errorColumnIndex < 0) return;

    for (int row = 0; row < n; ++row) {
        const int cnt = m_model->rowErrorCountCached(row);
        if (cnt <= 1) continue;

        int h = lineHeight * cnt + 4;
        if (h > maxHeight) h = maxHeight;

        ui->tableData->setRowHeight(row, h);
        m_expandedRows.append(row);
    }
}

void DataViewDialog::startAsyncLoad()
{
    // ✅ Если предыдущая загрузка ещё идёт — отменяем её
    if (m_loadingActive) {
        qDebug() << "[DataViewDialog] Отмена предыдущей загрузки";
        if (m_loaderWorker) {
            m_loaderWorker->cancel();
        }
        if (m_loaderThread && m_loaderThread->isRunning()) {
            m_loaderThread->quit();
            m_loaderThread->wait(2000);
        }
        m_loaderWorker = nullptr;
        m_loaderThread = nullptr;
        m_loadingActive = false;
    }

    // ✅ Блокируем кнопки в родительском окне
    ui->btnLoad->setEnabled(false);
    ui->btnExport->setEnabled(false);
    ui->statusLabel->setText("Загрузка...");

    m_loadingActive = true;
    m_totalRowsToLoad = 0;

    // ✅ Создаём (или переиспользуем) диалог прогресса.
    //    НЕ используем parent = this — во избежание двойного удаления.
    if (!m_progressDialog) {
        m_progressDialog = new ProgressDialog(nullptr);
        // Центрируем относительно главного окна
        if (QWidget *top = window()) {
            QRect g = top->geometry();
            m_progressDialog->move(
                g.center() - QPoint(m_progressDialog->width()  / 2,
                                    m_progressDialog->height() / 2));
        }
    }
    m_progressDialog->resetCancelled();
    m_progressDialog->setTitle("Загрузка данных из БД");
    m_progressDialog->setPhase("Подключение к базе данных...");
    m_progressDialog->setRange(0, 0);          // неопределённый
    m_progressDialog->setFormat("Подготовка...");
    m_progressDialog->show();

    // Конфиг БД для воркера
    QJsonObject dbConfig;
    dbConfig["host"]     = m_dbConfig.value("host");
    dbConfig["port"]     = m_dbConfig.value("port");
    dbConfig["dbName"]   = m_dbConfig.value("dbName");
    dbConfig["user"]     = m_dbConfig.value("user");
    dbConfig["password"] = m_dbConfig.value("password");

    // Куда загружать
    const QString safeTableName = getSafeTableName(m_filePath);
    const QTime tStart = m_timeStart.isValid() ? m_timeStart : QTime(0, 0, 0);
    const QTime tEnd   = m_timeEnd.isValid()   ? m_timeEnd   : QTime(23, 59, 59);
    const QDateTime startDt(ui->dateStart->date(), tStart);
    const QDateTime endDt(ui->dateEnd->date(), tEnd);

    // Создаём поток и воркер
    m_loaderThread = new QThread(this);
    m_loaderWorker = new DbLoaderWorker(dbConfig);
    m_loaderWorker->moveToThread(m_loaderThread);

    connect(m_loaderThread, &QThread::started,
            m_loaderWorker, &DbLoaderWorker::load);
    connect(m_loaderWorker, &DbLoaderWorker::totalRowsKnown,
            this, &DataViewDialog::onDbLoadTotalRows);
    connect(m_loaderWorker, &DbLoaderWorker::progress,
            this, &DataViewDialog::onDbLoadProgress);
    connect(m_loaderWorker, &DbLoaderWorker::finished,
            this, &DataViewDialog::onDbLoadFinished);
    connect(m_loaderWorker, &DbLoaderWorker::failed,
            this, &DataViewDialog::onDbLoadFailed);

    connect(m_loaderThread, &QThread::finished,
            m_loaderWorker, &QObject::deleteLater);
    connect(m_loaderThread, &QThread::finished,
            m_loaderThread, &QObject::deleteLater);
    connect(m_loaderThread, &QThread::finished,
            this, [this]() {
                m_loaderThread = nullptr;
                m_loaderWorker = nullptr;
            });

    m_loaderWorker->setLoadParams(safeTableName, startDt, endDt);
    m_loaderThread->start();
}

void DataViewDialog::onDbLoadProgress(int rowsRead)
{
    if (!m_progressDialog) return;

    m_progressDialog->setPhase(
        QString("Чтение строк из БД: %1").arg(rowsRead));
    if (m_totalRowsToLoad > 0) {
        m_progressDialog->setValue(rowsRead);
    }
    ui->statusLabel->setText(
        QString("Загрузка... %1 из %2")
            .arg(rowsRead)
            .arg(m_totalRowsToLoad));

    // ✅ Даём UI шанс перерисоваться — иначе кнопка "Отмена" не нажимается.
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    // ✅ Проверяем, не нажал ли пользователь "Отмена"
    if (m_progressDialog->wasCancelled() && m_loaderWorker) {
        m_loaderWorker->cancel();
    }
}

void DataViewDialog::onDbLoadTotalRows(int totalRows)
{
    m_totalRowsToLoad = totalRows;
    qDebug() << "[DataViewDialog] Общее количество строк:" << totalRows;

    if (!m_progressDialog) return;

    if (totalRows == 0) {
        m_progressDialog->setRange(0, 1);
        m_progressDialog->setValue(1);
        m_progressDialog->setFormat("Нет данных");
        m_progressDialog->setPhase("За выбранный диапазон записей не найдено");
        ui->statusLabel->setText("Нет данных за выбранный диапазон");
        return;
    }

    m_progressDialog->setRange(0, totalRows);
    m_progressDialog->setValue(0);
    m_progressDialog->setFormat("%v из %m (%p%)");
    m_progressDialog->setPhase(
        QString("Найдено строк: %1. Начинаем чтение...").arg(totalRows));
    ui->statusLabel->setText(QString("Загрузка... 0 из %1").arg(totalRows));
}


void DataViewDialog::onDbLoadFinished(const QStringList &columns,
                                      const QVector<MeasurementsModel::Row> &rows,
                                      int timeColumnIndex,
                                      int errorColumnIndex,
                                      const QVector<QPair<double, double>> &columnRanges)
{
    QElapsedTimer perf;
    perf.start();

    qDebug() << "[DataViewDialog] Загрузка завершена, строк:" << rows.size();

    // ========================================================================
    // ШАГ 0: Стоп рабочего потока (быстро, делаем сразу)
    // ========================================================================
    if (m_loaderThread && m_loaderThread->isRunning()) {
        m_loaderThread->quit();
        m_loaderThread->wait(2000);
    }
    qDebug() << "[PERF] Стоп потока:" << perf.restart() << "мс";

    // ✅ Проверяем, не была ли загрузка отменена пользователем
    const bool userCancelled = (m_progressDialog && m_progressDialog->wasCancelled());

    // ========================================================================
    // Случай "нет данных" — приходит из воркера с пустыми columns
    // ========================================================================
    if (userCancelled) {
        m_model->beginBulkInsert();
        m_model->setColumnNames({});
        m_model->endBulkInsert();
        m_columnRanges.clear();
        m_dataProxy->invalidate();
        m_journalProxy->invalidate();

        closeProgressDialog();
        ui->btnLoad->setEnabled(true);
        ui->btnExport->setEnabled(true);
        ui->statusLabel->setText("Загрузка отменена пользователем");

        m_loadingActive = false;
        m_totalRowsToLoad = 0;
        return;
    }

    if (columns.isEmpty() || timeColumnIndex < 0 || errorColumnIndex < 0) {
        m_model->beginBulkInsert();
        m_model->setColumnNames({});
        m_model->endBulkInsert();
        m_columnRanges.clear();
        m_dataProxy->invalidate();
        m_journalProxy->invalidate();

        closeProgressDialog();
        ui->btnLoad->setEnabled(true);
        ui->btnExport->setEnabled(true);
        ui->statusLabel->setText("Нет данных за выбранный диапазон");

        m_loadingActive = false;
        m_totalRowsToLoad = 0;

        QMessageBox::information(this, "Нет данных",
            "За выбранный диапазон дат записей не найдено.");
        return;
    }

    // ========================================================================
    // Обновляем прогресс-диалог: перешли к обработке
    // ========================================================================
    if (m_progressDialog) {
        m_progressDialog->setPhase("Обработка полученных данных...");
        m_progressDialog->setRange(0, 5);
        m_progressDialog->setValue(0);
        m_progressDialog->setFormat("Шаг %v из %m");
    }
    QApplication::processEvents();

    // ========================================================================
    // ШАГ 1: Настройка модели и прокси
    // ========================================================================
    if (!updateProgressAndProcess("Настройка модели...", 1)) {
        onDbLoadFinished(QStringList(), {}, -1, -1, {}); // рекурсивно обработает "отмену"
        return;
    }

    m_timeColumnIndex  = timeColumnIndex;
    m_errorColumnIndex = errorColumnIndex;
    m_columnRanges     = columnRanges;

    m_model->setColumnNames(columns);
    m_model->setErrorColumn(errorColumnIndex);

    m_journalProxy->setTimeColumn(timeColumnIndex);
    m_journalProxy->setErrorColumn(errorColumnIndex);
    m_journalProxy->setVisibleColumns({timeColumnIndex, errorColumnIndex});

    qDebug() << "[PERF] Настройка модели и прокси:" << perf.restart() << "мс";

    // ========================================================================
    // ШАГ 2: Загрузка данных в модель
    // ========================================================================
    if (!updateProgressAndProcess(
            QString("Загрузка %1 строк в модель...").arg(rows.size()), 2)) {
        onDbLoadFinished(QStringList(), {}, -1, -1, {});
        return;
    }

    m_model->beginBulkInsert();
    for (const auto &row : rows) {
        m_model->appendRow(row);
    }
    m_model->endBulkInsert();
    qDebug() << "[PERF] appendRow (все строки):" << perf.restart() << "мс";

    // ========================================================================
    // ШАГ 3: Инвалидация прокси
    // ========================================================================
    if (!updateProgressAndProcess("Обновление прокси-моделей...", 3)) {
        onDbLoadFinished(QStringList(), {}, -1, -1, {});
        return;
    }

    m_dataProxy->invalidate();
    m_journalProxy->invalidate();
    ui->tableJournal->reset();
    qDebug() << "[PERF] invalidate + tableJournal->reset:" << perf.restart() << "мс";

    // ========================================================================
    // ШАГ 4: Пересчёт высот строк в tableData
    // ========================================================================
    if (!updateProgressAndProcess("Пересчёт высот строк...", 4)) {
        onDbLoadFinished(QStringList(), {}, -1, -1, {});
        return;
    }

    {
        QElapsedTimer t;
        t.start();
        recalcRowHeightsTableData();
        qDebug() << "[PERF] recalcRowHeightsTableData:" << t.elapsed() << "мс";
    }

    // ========================================================================
    // ШАГ 5: Сброс меток + синхронизация видимых столбцов
    // ========================================================================
    if (!updateProgressAndProcess("Синхронизация каналов...", 5)) {
        onDbLoadFinished(QStringList(), {}, -1, -1, {});
        return;
    }

    {
        QElapsedTimer t;
        t.start();
        resetMarkers(false);
        syncVisibleColumnsFromTree();
        qDebug() << "[PERF] resetMarkers + syncVisibleColumnsFromTree:"
                 << t.elapsed() << "мс";
    }

    // ========================================================================
    // ШАГ 6: Ширины столбцов
    // ========================================================================
    {
        QElapsedTimer t;
        t.start();
        applyColumnWidths();
        qDebug() << "[PERF] applyColumnWidths:" << t.elapsed() << "мс";
    }

    // ========================================================================
    // ШАГ 7: Построение графиков
    // ========================================================================
    if (m_progressDialog) {
        m_progressDialog->setPhase("Построение графиков...");
    }
    QApplication::processEvents();

    {
        QElapsedTimer t;
        t.start();
        m_yRangeValid = false;
        buildGraphs();
        qDebug() << "[PERF] buildGraphs:" << t.elapsed() << "мс";
    }

    // ========================================================================
    // ШАГ 8 (ФИНАЛЬНЫЙ): Скрыть прогресс, разблокировать UI, обновить статус
    // ========================================================================
    closeProgressDialog();

    ui->btnLoad->setEnabled(true);
    ui->btnExport->setEnabled(true);
    ui->statusLabel->setText(
        QString("Загружено: %1 записей; аварийных: %2")
            .arg(rows.size())
            .arg(m_journalProxy->rowCount()));

    m_loadingActive = false;
    m_totalRowsToLoad = 0;

    qDebug() << "[PERF] ---------- ИТОГО onDbLoadFinished:"
             << perf.elapsed() << "мс ----------";
}


void DataViewDialog::onDbLoadFailed(const QString &errorMessage)
{
    qWarning() << "[DataViewDialog] Ошибка загрузки:" << errorMessage;

    // Стоп поток
    if (m_loaderThread && m_loaderThread->isRunning()) {
        m_loaderThread->quit();
        m_loaderThread->wait(2000);
    }

    // ✅ Проверяем, не отмена ли это (воркер эмитит failed("Загрузка отменена..."))
    const bool wasCancelled = (m_progressDialog && m_progressDialog->wasCancelled())
                              || errorMessage.contains("отменена", Qt::CaseInsensitive);

    closeProgressDialog();
    ui->btnLoad->setEnabled(true);
    ui->btnExport->setEnabled(true);

    m_loadingActive = false;
    m_totalRowsToLoad = 0;

    if (wasCancelled) {
        ui->statusLabel->setText("Загрузка отменена пользователем");
        // Не показываем QMessageBox — пользователь сам отменил.
        return;
    }

    ui->statusLabel->setText("Ошибка: " + errorMessage);
    QMessageBox::warning(this, "Ошибка загрузки", errorMessage);
}

QJsonObject DataViewDialog::loadDbConfigFromFile()
{
    QJsonObject result;
    QString configPath = QCoreApplication::applicationDirPath() + "/rmo_config.json";
    QFile file(configPath);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject root = doc.object();
        QJsonObject dbConfig = root.value("database").toObject();

        result["host"]     = dbConfig.value("host").toString("localhost");
        result["port"]     = dbConfig.value("port").toInt(5432);
        result["dbName"]   = dbConfig.value("dbName").toString("rmo_db");
        result["user"]     = dbConfig.value("user").toString("postgres");

        QString encPass = dbConfig.value("password").toString();
        result["password"] = encPass.isEmpty() ? "" : CryptoHelper::decrypt(encPass);
    }
    return result;
}

bool DataViewDialog::updateProgressAndProcess(const QString &phase, int value)
{
    // ✅ Если загрузка уже завершена/отменена — не продолжаем
    if (!m_loadingActive) return false;
    if (!m_progressDialog) return true;

    m_progressDialog->setPhase(phase);
    if (value >= 0) {
        m_progressDialog->setValue(value);
    }

    // ✅ Обрабатываем ТОЛЬКО события отрисовки и таймеров,
    //    исключая пользовательский ввод (клики по другим окнам,
    //    которые могут привести к закрытию DataViewDialog).
    //    При этом кнопка "Отмена" в самом ProgressDialog
    //    обрабатывается — потому что для модального окна
    //    mouseEvents идут напрямую в него, минуя очередь.
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    // ✅ Дополнительно: даём обработать события именно диалогу прогресса,
    //    чтобы клик по "Отмена" сработал даже при ExcludeUserInputEvents.
    if (m_progressDialog && m_progressDialog->isVisible()) {
        QApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    // ✅ Проверяем, что `this` ещё жив и загрузка не отменена
    if (!m_loadingActive) return false;
    if (m_progressDialog && m_progressDialog->wasCancelled()) return false;

    return true;
}

// ============================================================================
// ✅ Закрыть диалог прогресса (не удаляя — переиспользуем при след. загрузке)
// ============================================================================
void DataViewDialog::closeProgressDialog()
{
    if (m_progressDialog) {
        m_progressDialog->hide();
    }
}

// ============================================================================
// ✅ Уничтожить диалог прогресса (только из деструктора)
// ============================================================================
void DataViewDialog::destroyProgressDialog()
{
    if (m_progressDialog) {
        m_progressDialog->hide();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void DataViewDialog::updateCalendarHighlights()
{
    const QDate today = QDate::currentDate();

    QTextCharFormat fmt;
    fmt.setBackground(QColor("#FFEB3B"));   // жёлтый фон
    fmt.setForeground(Qt::black);
    fmt.setFontWeight(QFont::Bold);

    auto applyToDateEdit = [&](QDateEdit *edit) {
        if (!edit) return;
        QCalendarWidget *cal = edit->calendarWidget();
        if (!cal) return;

        // Сброс всех ранее установленных форматов (на случай повторного вызова)
        cal->setDateTextFormat(QDate(), QTextCharFormat());

        // Подсветка сегодняшней даты
        cal->setDateTextFormat(today, fmt);
    };

    applyToDateEdit(ui->dateStart);
    applyToDateEdit(ui->dateEnd);
}
