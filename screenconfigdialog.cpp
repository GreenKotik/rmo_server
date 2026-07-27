#include "screenconfigdialog.h"
#include "ui_screenconfigdialog.h"
#include <QPushButton>          // ⬅️ Обязательно для button()->setText()
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHeaderView>

ScreenConfigDialog::ScreenConfigDialog(int columns, const QStringList &docPaths, QWidget *parent)
    : QDialog(parent), ui(new Ui::ScreenConfigDialog)
{
    ui->setupUi(this);
    resize(750, 520);

    // ✅ Принудительно задаём русский текст кнопок
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("ОК");
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText("Отмена");

    // 1. Настраиваем таблицу из .ui
    ui->tableDocuments->setDragDropMode(QAbstractItemView::NoDragDrop);
    ui->tableDocuments->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableDocuments->verticalHeader()->setVisible(false);
    ui->tableDocuments->verticalHeader()->setDefaultSectionSize(32);
    ui->tableDocuments->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableDocuments->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->tableDocuments->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableDocuments->setShowGrid(true);

    // ✅ Границы ячеек: сплошная линия, чёрный цвет
    ui->tableDocuments->setGridStyle(Qt::SolidLine);
    ui->tableDocuments->setStyleSheet("QTableWidget { gridline-color: black; }");

    ui->tableDocuments->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->tableDocuments->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 2. Устанавливаем Event Filter на VIEWPORT (где происходит рендеринг и обработка мыши)
    ui->tableDocuments->viewport()->installEventFilter(this);

    // 3. Заставляем layout растягивать таблицу на всё доступное место
    int tableIndex = ui->verticalLayout->indexOf(ui->tableDocuments);
    if (tableIndex != -1) {
        ui->verticalLayout->setStretch(tableIndex, 1);
    }

    // Инициализация
    ui->spinColumns->setValue(columns);
    rebuildTable(columns, docPaths);

    connect(ui->spinColumns, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ScreenConfigDialog::onColumnsChanged);
}

ScreenConfigDialog::~ScreenConfigDialog()
{
    delete ui;
}

// Перехват событий мыши для реализации обмена ячеек
bool ScreenConfigDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->tableDocuments->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragStartPos = me->pos();
                QModelIndex idx = ui->tableDocuments->indexAt(me->pos());
                if (idx.isValid()) {
                    m_dragStartRow = idx.row();
                    m_dragStartCol = idx.column();
                    m_isDragging = false;
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton && !m_isDragging) {
                int dist = (me->pos() - m_dragStartPos).manhattanLength();
                if (dist >= QApplication::startDragDistance()) {
                    m_isDragging = true;
                    ui->tableDocuments->setCursor(Qt::ClosedHandCursor);
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (m_isDragging && me->button() == Qt::LeftButton) {
                ui->tableDocuments->setCursor(Qt::ArrowCursor);
                QModelIndex dropIdx = ui->tableDocuments->indexAt(me->pos());

                if (dropIdx.isValid() && m_dragStartRow != -1) {
                    int tgtR = dropIdx.row();
                    int tgtC = dropIdx.column();

                    // Обмен только если ячейки разные
                    if (m_dragStartRow != tgtR || m_dragStartCol != tgtC) {
                        QTableWidgetItem *src = ui->tableDocuments->takeItem(m_dragStartRow, m_dragStartCol);
                        QTableWidgetItem *tgt = ui->tableDocuments->takeItem(tgtR, tgtC);

                        // Безопасный обмен с сохранением всех данных (Qt::UserRole, флаги и т.д.)
                        ui->tableDocuments->setItem(tgtR, tgtC, src ? src : new QTableWidgetItem());
                        ui->tableDocuments->setItem(m_dragStartRow, m_dragStartCol, tgt ? tgt : new QTableWidgetItem());
                    }
                }
                m_isDragging = false;
            }
        }
    }
    // Возвращаем false, чтобы таблица могла обработать выделение, фокус и т.д.
    return QDialog::eventFilter(watched, event);
}

void ScreenConfigDialog::onColumnsChanged(int newCols)
{
    QStringList currentPaths = docPaths();
    rebuildTable(qMax(1, newCols), currentPaths);
}

void ScreenConfigDialog::rebuildTable(int cols, const QStringList &paths)
{
    int count = paths.size();
    int rows = (count + cols - 1) / cols;

    ui->tableDocuments->setColumnCount(cols);
    ui->tableDocuments->setRowCount(rows);
    ui->tableDocuments->clearContents();

    for (int i = 0; i < count; ++i) {
        int r = i / cols;
        int c = i % cols;

        QString fullPath = paths.at(i);
        QString fileName = QFileInfo(fullPath).fileName();

        QTableWidgetItem *item = new QTableWidgetItem(fileName);
        item->setData(Qt::UserRole, fullPath);
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        ui->tableDocuments->setItem(r, c, item);
    }

    ui->tableDocuments->resizeRowsToContents();
}

int ScreenConfigDialog::columns() const
{
    return ui->spinColumns->value();
}

QStringList ScreenConfigDialog::docPaths() const
{
    QStringList result;
    int cols = ui->tableDocuments->columnCount();
    int rows = ui->tableDocuments->rowCount();

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            QTableWidgetItem *item = ui->tableDocuments->item(r, c);
            if (item) {
                QString fullPath = item->data(Qt::UserRole).toString();
                if (!fullPath.isEmpty()) {
                    result << fullPath;
                }
            }
        }
    }
    return result;
}
