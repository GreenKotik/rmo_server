#include "screenconfigdialog.h"
#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>

// ============================================================================
// КОНСТРУКТОР
// ============================================================================
ScreenConfigDialog::ScreenConfigDialog(const QList<DocGroup> &groups, QWidget *parent)
    : QDialog(parent)
    , m_groups(groups)
{
    setWindowTitle("Конфигурация экрана");
    setMinimumSize(900, 600);
    resize(1000, 650);

    // ================================================================
    // Основной вертикальный layout
    // ================================================================
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    // ================================================================
    // Верхняя часть: две колонки (группы + документы)
    // ================================================================
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(10);

    // ---- Левая колонка: список групп ----
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(6);

    leftLayout->addWidget(new QLabel("Группы (перетащите документ сюда):", this));

    m_groupsList = new QListWidget(this);
    m_groupsList->setDragDropMode(QAbstractItemView::InternalMove);
    m_groupsList->setDefaultDropAction(Qt::MoveAction);
    m_groupsList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_groupsList->setAcceptDrops(true);
    m_groupsList->viewport()->setAcceptDrops(true);
    m_groupsList->installEventFilter(this);                 // ✅ добавить
    m_groupsList->viewport()->installEventFilter(this);
    leftLayout->addWidget(m_groupsList, 1);

    QHBoxLayout *groupBtnLayout = new QHBoxLayout();
    groupBtnLayout->setSpacing(4);
    m_addGroupBtn    = new QPushButton("+ Группа", this);
    m_removeGroupBtn = new QPushButton("− Группа", this);
    m_renameGroupBtn = new QPushButton("✎ Переименовать", this);
    groupBtnLayout->addWidget(m_addGroupBtn);
    groupBtnLayout->addWidget(m_removeGroupBtn);
    groupBtnLayout->addWidget(m_renameGroupBtn);
    leftLayout->addLayout(groupBtnLayout);

    // ---- Правая колонка: документы ----
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(6);

    rightLayout->addWidget(new QLabel("Документы выбранной группы:", this));

    m_documentsTable = new QTableWidget(this);
    m_documentsTable->setColumnCount(1);
    m_documentsTable->setHorizontalHeaderLabels({ "Документ" });
    m_documentsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_documentsTable->verticalHeader()->setVisible(false);
    m_documentsTable->verticalHeader()->setDefaultSectionSize(32);
    m_documentsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_documentsTable->setSelectionBehavior(QAbstractItemView::SelectItems);   // ← ВАЖНО
    m_documentsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_documentsTable->setShowGrid(true);
    m_documentsTable->setGridStyle(Qt::SolidLine);
    m_documentsTable->setStyleSheet(
        "QTableWidget { gridline-color: black; }"
        "QHeaderView::section:horizontal {"
        "    background-color: #E8F0F8;"   /* голубой фон */
        "    color: #000000;"
        "    padding: 4px;"
        "    border: 1px solid #B0B0B0;"
        "    font-weight: bold;"
        "}"
    );
    // ✅ Собственный DnD через eventFilter — штатный DnD выключён.
    m_documentsTable->setDragEnabled(false);
    m_documentsTable->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_documentsTable->setAcceptDrops(true);
    m_documentsTable->viewport()->setAcceptDrops(true);
    m_documentsTable->viewport()->installEventFilter(this);
    rightLayout->addWidget(m_documentsTable, 1);

    QHBoxLayout *columnsLayout = new QHBoxLayout();
    columnsLayout->addWidget(new QLabel("Количество колонок:", this));
    m_columnsSpin = new QSpinBox(this);
    m_columnsSpin->setRange(1, 10);
    m_columnsSpin->setValue(2);
    columnsLayout->addWidget(m_columnsSpin);
    columnsLayout->addStretch();
    rightLayout->addLayout(columnsLayout);

    contentLayout->addLayout(leftLayout, 1);
    contentLayout->addLayout(rightLayout, 2);
    rootLayout->addLayout(contentLayout, 1);

    // ================================================================
    // Нижняя часть: OK / Cancel
    // ================================================================
    QHBoxLayout *okLayout = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton("ОК", this);
    okBtn->setDefault(true);
    QPushButton *cancelBtn = new QPushButton("Отмена", this);
    okLayout->addStretch();
    okLayout->addWidget(okBtn);
    okLayout->addWidget(cancelBtn);
    rootLayout->addLayout(okLayout);

    // ================================================================
    // Заполнение и подключения
    // ================================================================
    connect(m_groupsList, &QListWidget::currentRowChanged,
            this, &ScreenConfigDialog::onGroupSelectionChanged);
    connect(m_groupsList->model(), &QAbstractItemModel::rowsMoved,
            this, &ScreenConfigDialog::onGroupOrderChanged);
    connect(m_addGroupBtn, &QPushButton::clicked,
            this, &ScreenConfigDialog::onAddGroupClicked);
    connect(m_removeGroupBtn, &QPushButton::clicked,
            this, &ScreenConfigDialog::onRemoveGroupClicked);
    connect(m_renameGroupBtn, &QPushButton::clicked,
            this, &ScreenConfigDialog::onRenameGroupClicked);
    connect(m_columnsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ScreenConfigDialog::onColumnsChanged);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // ✅ Теперь заполняем и выбираем первую группу.
    rebuildGroupsList();
    if (!m_groups.isEmpty()) {
        m_groupsList->setCurrentRow(0);
        // Гарантированно обновляем таблицу (сигнал мог не эмититься).
        onGroupSelectionChanged(0);
    } else {
        refreshDocumentTable(-1);
    }
    updateButtonsState();
}

ScreenConfigDialog::~ScreenConfigDialog()
{
}

void ScreenConfigDialog::rebuildGroupsList()
{
    m_groupsList->blockSignals(true);
    m_groupsList->clear();
    for (const DocGroup &g : m_groups) {
        QListWidgetItem *item = new QListWidgetItem(g.name, m_groupsList);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    }
    m_groupsList->blockSignals(false);
}

void ScreenConfigDialog::refreshDocumentTable(int groupIndex)
{
    m_documentsTable->setRowCount(0);
    m_documentsTable->setColumnCount(1);

    if (groupIndex < 0 || groupIndex >= m_groups.size()) {
        m_documentsTable->setHorizontalHeaderLabels({ "Документ" });
        m_columnsSpin->blockSignals(true);
        m_columnsSpin->setValue(2);
        m_columnsSpin->blockSignals(false);
        return;
    }

    const DocGroup &g = m_groups[groupIndex];
    const int cols = qMax(1, g.columns);

    // ✅ Настраиваем таблицу под сетку cols × rows
    m_documentsTable->setColumnCount(cols);

    // Заголовки столбцов — нейтральные ("Документ N"), чтобы не сбивать с толку.
    QStringList headers;
    for (int c = 0; c < cols; ++c) {
        headers << QString("Колонка %1").arg(c + 1);
    }
    m_documentsTable->setHorizontalHeaderLabels(headers);

    m_columnsSpin->blockSignals(true);
    m_columnsSpin->setValue(g.columns);
    m_columnsSpin->blockSignals(false);

    const int count = g.docPaths.size();
    const int rows = (count + cols - 1) / cols;
    m_documentsTable->setRowCount(rows);

    for (int i = 0; i < count; ++i) {
        const QString &path = g.docPaths[i];
        const QString fileName = QFileInfo(path).fileName();

        QTableWidgetItem *item = new QTableWidgetItem(fileName);
        item->setData(Qt::UserRole, path);              // полный путь
        item->setData(Qt::UserRole + 1, i);             // исходный индекс в g.docPaths
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        const int r = i / cols;
        const int c = i % cols;
        m_documentsTable->setItem(r, c, item);
    }

    // Подгоняем ширины столбцов, чтобы всё было видно.
    for (int c = 0; c < cols; ++c) {
        m_documentsTable->setColumnWidth(c, m_documentsTable->width() / cols);
    }
}

void ScreenConfigDialog::onGroupSelectionChanged(int row)
{
    m_currentGroupIndex = row;
    refreshDocumentTable(row);
    updateButtonsState();
}

void ScreenConfigDialog::onGroupOrderChanged()
{
    // Собираем новый порядок по текущему списку.
    QList<DocGroup> newOrder;
    for (int i = 0; i < m_groupsList->count(); ++i) {
        const QString name = m_groupsList->item(i)->text();
        // Ищем группу с этим именем в старом списке
        for (const DocGroup &g : m_groups) {
            if (g.name == name) {
                newOrder.append(g);
                break;
            }
        }
    }
    // Если что-то потерялось — восстанавливаем из исходного.
    if (newOrder.size() == m_groups.size()) {
        m_groups = newOrder;
    }
}

void ScreenConfigDialog::onAddGroupClicked()
{
    if (m_groups.size() >= 20) {
        QMessageBox::warning(this, "Ограничение",
            "Достигнут предел количества групп (20).");
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(this, "Новая группа",
        "Имя группы:", QLineEdit::Normal,
        QString("Группа %1").arg(m_groups.size() + 1), &ok);
    if (!ok) return;

    name = name.trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Имя группы не может быть пустым.");
        return;
    }
    for (const DocGroup &g : m_groups) {
        if (g.name == name) {
            QMessageBox::warning(this, "Ошибка",
                QString("Группа с именем '%1' уже существует.").arg(name));
            return;
        }
    }

    DocGroup newGroup;
    newGroup.name = name;
    newGroup.columns = 2;

    m_groups.append(newGroup);
    rebuildGroupsList();
    m_groupsList->setCurrentRow(m_groups.size() - 1);
    onGroupSelectionChanged(m_groups.size() - 1);
    updateButtonsState();
}

void ScreenConfigDialog::onRemoveGroupClicked()
{
    const int row = m_groupsList->currentRow();
    if (row < 0 || row >= m_groups.size()) return;

    if (m_groups.size() <= 1) {
        QMessageBox::warning(this, "Ошибка",
            "Нельзя удалить последнюю группу.");
        return;
    }

    const DocGroup &g = m_groups[row];
    if (!g.docPaths.isEmpty()) {
        QMessageBox::warning(this, "Ошибка",
            QString("Группа '%1' содержит документы.\n"
                    "Сначала перенесите их в другую группу.").arg(g.name));
        return;
    }

    if (QMessageBox::question(this, "Подтверждение",
            QString("Удалить группу '%1'?").arg(g.name),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    m_groups.removeAt(row);
    rebuildGroupsList();
    if (!m_groups.isEmpty()) {
        int newRow = qMin(row, m_groups.size() - 1);
        m_groupsList->setCurrentRow(newRow);
        onGroupSelectionChanged(newRow);
    } else {
        m_currentGroupIndex = -1;
        refreshDocumentTable(-1);
    }
    updateButtonsState();
}

void ScreenConfigDialog::onRenameGroupClicked()
{
    const int row = m_groupsList->currentRow();
    if (row < 0 || row >= m_groups.size()) return;

    if (!m_groups[row].docPaths.isEmpty()) {
        QMessageBox::warning(this, "Ошибка",
            "Нельзя переименовать группу с документами.\n"
            "Сначала перенесите их в другую группу.");
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(this, "Переименование",
        "Новое имя группы:", QLineEdit::Normal, m_groups[row].name, &ok);
    if (!ok) return;

    name = name.trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Имя группы не может быть пустым.");
        return;
    }
    for (int i = 0; i < m_groups.size(); ++i) {
        if (i != row && m_groups[i].name == name) {
            QMessageBox::warning(this, "Ошибка",
                QString("Группа с именем '%1' уже существует.").arg(name));
            return;
        }
    }

    m_groups[row].name = name;
    m_groupsList->item(row)->setText(name);
}

void ScreenConfigDialog::onColumnsChanged(int value)
{
    if (m_currentGroupIndex < 0 || m_currentGroupIndex >= m_groups.size()) return;
    m_groups[m_currentGroupIndex].columns = value;
    // ✅ Перестраиваем таблицу документов в новое число колонок.
    refreshDocumentTable(m_currentGroupIndex);
}

void ScreenConfigDialog::updateButtonsState()
{
    const bool hasGroup = m_currentGroupIndex >= 0;
    m_removeGroupBtn->setEnabled(hasGroup && m_groups.size() > 1);
    m_renameGroupBtn->setEnabled(hasGroup);
}

bool ScreenConfigDialog::eventFilter(QObject *watched, QEvent *event)
{
    // ========================================================================
    // ✅ ТАБЛИЦА ДОКУМЕНТОВ — старт drag + drop внутри таблицы
    // ========================================================================
    if (watched == m_documentsTable->viewport()) {
        // --- Старт drag ---
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                QModelIndex idx = m_documentsTable->indexAt(me->pos());
                if (idx.isValid() && m_documentsTable->item(idx.row(), idx.column())) {
                    m_dragDocRow = idx.row();
                    m_dragDocCol = idx.column();
                    m_dragStartPos = me->pos();
                } else {
                    m_dragDocRow = -1;
                    m_dragDocCol = -1;
                }
            }
            return false;
        }
        else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if ((me->buttons() & Qt::LeftButton) &&
                m_dragDocRow >= 0 && m_dragDocCol >= 0 &&
                (me->pos() - m_dragStartPos).manhattanLength()
                    >= QApplication::startDragDistance()) {

                if (m_currentGroupIndex < 0 ||
                    m_currentGroupIndex >= m_groups.size()) return false;

                QTableWidgetItem *item = m_documentsTable->item(m_dragDocRow, m_dragDocCol);
                if (!item) return false;

                const QString path = item->data(Qt::UserRole).toString();
                if (path.isEmpty()) return false;

                QMimeData *mime = new QMimeData();
                mime->setData("application/x-rmo-docpath", path.toUtf8());

                QDrag *drag = new QDrag(this);
                drag->setMimeData(mime);

                m_dragDocRow = -1;
                m_dragDocCol = -1;
                drag->exec(Qt::MoveAction);
                return true;
            }
            return false;
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragDocRow = -1;
            m_dragDocCol = -1;
            return false;
        }
        // --- Drop внутри таблицы ---
        else if (event->type() == QEvent::DragEnter ||
                 event->type() == QEvent::DragMove) {
            QDragMoveEvent *de = static_cast<QDragMoveEvent*>(event);
            if (de->mimeData()->hasFormat("application/x-rmo-docpath")) {
                de->acceptProposedAction();
                return true;
            }
            return false;
        }
        else if (event->type() == QEvent::Drop) {
            QDropEvent *de = static_cast<QDropEvent*>(event);
            if (!de->mimeData()->hasFormat("application/x-rmo-docpath")) {
                return false;
            }

            const QString path = QString::fromUtf8(
                de->mimeData()->data("application/x-rmo-docpath"));
            if (path.isEmpty()) { de->ignore(); return true; }

            QModelIndex dropIdx = m_documentsTable->indexAt(de->pos());
            int targetRow, targetCol;
            const int cols = m_documentsTable->columnCount();
            if (cols <= 0) { de->ignore(); return true; }

            if (m_currentGroupIndex < 0 ||
                m_currentGroupIndex >= m_groups.size()) {
                de->ignore();
                return true;
            }
            DocGroup &g = m_groups[m_currentGroupIndex];

            if (dropIdx.isValid()) {
                targetRow = dropIdx.row();
                targetCol = dropIdx.column();
            } else {
                const int total = g.docPaths.size();
                targetRow = total / cols;
                targetCol = total % cols;
            }

            const int targetFlat = targetRow * cols + targetCol;
            const int srcFlat = g.docPaths.indexOf(path);
            if (srcFlat < 0) { de->ignore(); return true; }
            if (srcFlat == targetFlat) { de->acceptProposedAction(); return true; }

            if (targetFlat >= g.docPaths.size()) {
                // Пустая ячейка в конце — переносим в конец.
                if (srcFlat != g.docPaths.size() - 1) {
                    QString moved = g.docPaths.takeAt(srcFlat);
                    g.docPaths.append(moved);
                }
            } else {
                // ✅ Swap: меняем местами src и target.
                std::swap(g.docPaths[srcFlat], g.docPaths[targetFlat]);
            }

            refreshDocumentTable(m_currentGroupIndex);
            de->acceptProposedAction();
            return true;
        }
    }

    // ========================================================================
    // ✅ СПИСОК ГРУПП — drop документа из таблицы
    //    Обрабатываем и viewport, и сам список — на случай, если
    //    InternalMove перехватит события до viewport().
    // ========================================================================
    if (watched == m_groupsList || watched == m_groupsList->viewport()) {
        if (event->type() == QEvent::DragEnter ||
            event->type() == QEvent::DragMove) {
            QDragMoveEvent *de = static_cast<QDragMoveEvent*>(event);

            // Наш MIME — принимаем. Иначе — пусть InternalMove обрабатывает сам.
            if (de->mimeData()->hasFormat("application/x-rmo-docpath")) {
                de->acceptProposedAction();
                return true;
            }
            return false;
        }
        else if (event->type() == QEvent::Drop) {
            QDropEvent *de = static_cast<QDropEvent*>(event);

            if (!de->mimeData()->hasFormat("application/x-rmo-docpath")) {
                // Не наш drop — пусть QListWidget обработает InternalMove.
                return false;
            }

            const QString path = QString::fromUtf8(
                de->mimeData()->data("application/x-rmo-docpath"));
            if (path.isEmpty()) { de->ignore(); return true; }

            // Координаты относительно viewport (itemAt ожидает viewport).
            QPoint vpPos = (watched == m_groupsList)
                               ? m_groupsList->viewport()->mapFrom(m_groupsList, de->pos())
                               : de->pos();

            QListWidgetItem *targetItem = m_groupsList->itemAt(vpPos);
            if (!targetItem) { de->ignore(); return true; }

            const int targetGroup = m_groupsList->row(targetItem);
            if (targetGroup < 0) { de->ignore(); return true; }

            if (m_currentGroupIndex < 0 ||
                targetGroup == m_currentGroupIndex) {
                // Перетащили на ту же группу — ничего не делаем.
                de->acceptProposedAction();
                return true;
            }

            DocGroup &src = m_groups[m_currentGroupIndex];
            DocGroup &dst = m_groups[targetGroup];

            const int srcIdx = src.docPaths.indexOf(path);
            if (srcIdx < 0) { de->ignore(); return true; }

            src.docPaths.removeAt(srcIdx);
            if (!dst.docPaths.contains(path)) {
                dst.docPaths.append(path);
            }

            refreshDocumentTable(m_currentGroupIndex);
            updateButtonsState();

            de->acceptProposedAction();
            return true;
        }
    }

    return QDialog::eventFilter(watched, event);
}
