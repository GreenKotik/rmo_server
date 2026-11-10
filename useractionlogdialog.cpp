#include "useractionlogdialog.h"
#include "databasemanager.h"
#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QDebug>

UserActionLogDialog::UserActionLogDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Журнал действий пользователей");
    setMinimumSize(1200, 700);
    setupUi();
    loadFilterLists();

    // ✅ По умолчанию — сегодняшние сутки, как в MFC.
    m_dateStart->setDateTime(QDateTime(QDate::currentDate(), QTime(0, 0, 0)));
    m_dateEnd->setDateTime(QDateTime(QDate::currentDate(), QTime(23, 59, 59)));

    loadLog();
}

void UserActionLogDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // ----------------------------------------------------------------
    // Ряд 1: даты + чекбоксы
    // ----------------------------------------------------------------
    auto *dateRow = new QHBoxLayout();
    dateRow->setSpacing(8);

    m_checkTimeStart = new QCheckBox("С:", this);
    m_checkTimeStart->setChecked(true);
    m_dateStart = new QDateTimeEdit(this);
    m_dateStart->setDisplayFormat("dd.MM.yyyy HH:mm:ss");
    m_dateStart->setCalendarPopup(true);

    m_checkTimeEnd = new QCheckBox("По:", this);
    m_checkTimeEnd->setChecked(true);
    m_dateEnd = new QDateTimeEdit(this);
    m_dateEnd->setDisplayFormat("dd.MM.yyyy HH:mm:ss");
    m_dateEnd->setCalendarPopup(true);

    dateRow->addWidget(m_checkTimeStart);
    dateRow->addWidget(m_dateStart);
    dateRow->addWidget(m_checkTimeEnd);
    dateRow->addWidget(m_dateEnd);
    dateRow->addStretch();

    mainLayout->addLayout(dateRow);

    // ----------------------------------------------------------------
    // Ряд 2: три ComboBox + кнопка «Найти»
    // ----------------------------------------------------------------
    auto *comboRow = new QHBoxLayout();
    comboRow->setSpacing(8);

    comboRow->addWidget(new QLabel("Пользователь:", this));
    m_userFilter = new QComboBox(this);
    m_userFilter->setMinimumWidth(180);
    comboRow->addWidget(m_userFilter);

    comboRow->addWidget(new QLabel("Контролируемый объект:", this));
    m_stationFilter = new QComboBox(this);
    m_stationFilter->setMinimumWidth(220);
    comboRow->addWidget(m_stationFilter);

    comboRow->addWidget(new QLabel("Операция:", this));
    m_operationFilter = new QComboBox(this);
    m_operationFilter->setMinimumWidth(180);
    comboRow->addWidget(m_operationFilter);

    comboRow->addStretch();

    m_findButton = new QPushButton("🔍 Найти", this);
    m_findButton->setMinimumWidth(120);
    m_findButton->setStyleSheet(
        "QPushButton { background-color: #005A9E; color: white; "
        "padding: 6px 16px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #004A8E; }"
    );
    comboRow->addWidget(m_findButton);

    mainLayout->addLayout(comboRow);

    // ----------------------------------------------------------------
    // Таблица
    // ----------------------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({
        "Время", "Компьютер", "Пользователь",
        "Контролируемый объект", "Операция", "Действия пользователя"
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(true);

    QHeaderView *h = m_table->horizontalHeader();
    h->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    h->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    h->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    h->setSectionResizeMode(3, QHeaderView::Interactive);
    h->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    h->setSectionResizeMode(5, QHeaderView::Stretch);

    m_table->setColumnWidth(3, 220);

    m_table->setStyleSheet(
        "QTableWidget { gridline-color: #D0D0D0; }"
        "QHeaderView::section { background-color: #005A9E; color: white; "
        "                        padding: 6px; font-weight: bold; }"
    );

    mainLayout->addWidget(m_table, /*stretch=*/1);

    // ----------------------------------------------------------------
    // Подключения
    // ----------------------------------------------------------------
    connect(m_findButton, &QPushButton::clicked,
            this, &UserActionLogDialog::onFindClicked);
    connect(m_checkTimeStart, &QCheckBox::toggled,
            m_dateStart, &QDateTimeEdit::setEnabled);
    connect(m_checkTimeEnd, &QCheckBox::toggled,
            m_dateEnd, &QDateTimeEdit::setEnabled);
}

void UserActionLogDialog::loadFilterLists()
{
    // --- Пользователи ---
    m_userFilter->clear();
    m_userFilter->addItem("Все");
    m_userFilter->addItems(DatabaseManager::instance().getUsernames());

    // --- Станции: список имён документов из config.docPaths ---
    m_stationFilter->clear();
    m_stationFilter->addItem("Все");

    if (MainWindow *mainWin = qobject_cast<MainWindow*>(parent())) {
        // Получаем список путей документов из конфига.
        // (Метод getDocPaths() нужно будет добавить в MainWindow,
        //  если его нет — см. врезку ниже.)
        const QStringList docPaths = mainWin->getDocPaths();
        for (const QString &path : docPaths) {
            const QString name = QFileInfo(path).completeBaseName();
            if (!name.isEmpty() && !m_stationFilter->findText(name)) {
                m_stationFilter->addItem(name);
            }
        }
    }

    // --- Операции: фиксированный список ---
    m_operationFilter->clear();
    m_operationFilter->addItem("Все");
    m_operationFilter->addItems({
        "Пользователи",
        "Параметры",
        "Команды",
        "Администрирование",
        "Аварии"
    });
}

void UserActionLogDialog::onFindClicked()
{
    loadLog();
}

void UserActionLogDialog::loadLog()
{
    if (!DatabaseManager::instance().isConnected()) {
        QMessageBox::warning(this, "Ошибка",
            "База данных не подключена.");
        return;
    }

    // ----------------------------------------------------------------
    // Собираем WHERE-условия
    // ----------------------------------------------------------------
    QStringList whereParts;
    QVariantMap bindings;

    if (m_checkTimeStart->isChecked()) {
        whereParts << "timestamp >= :ts_start";
        bindings[":ts_start"] = m_dateStart->dateTime();
    }
    if (m_checkTimeEnd->isChecked()) {
        whereParts << "timestamp <= :ts_end";
        bindings[":ts_end"] = m_dateEnd->dateTime();
    }
    if (m_userFilter->currentIndex() > 0) {
        whereParts << "username = :user";
        bindings[":user"] = m_userFilter->currentText();
    }
    if (m_stationFilter->currentIndex() > 0) {
        whereParts << "station = :station";
        bindings[":station"] = m_stationFilter->currentText();
    }
    if (m_operationFilter->currentIndex() > 0) {
        whereParts << "operation = :operation";
        bindings[":operation"] = m_operationFilter->currentText();
    }

    QString sql = "SELECT timestamp, computer, username, station, "
                  "       operation, message "
                  "FROM user_actions_log";
    if (!whereParts.isEmpty()) {
        sql += " WHERE " + whereParts.join(" AND ");
    }
    sql += " ORDER BY timestamp ASC";
    // ✅ Берём на 1 больше лимита, чтобы понять — упёрлись или нет.
    sql += QString(" LIMIT %1").arg(kMaxRows + 1);

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(sql);
    for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
        query.bindValue(it.key(), it.value());
    }

    if (!query.exec()) {
        QMessageBox::critical(this, "Ошибка",
            "Не удалось загрузить журнал:\n" + query.lastError().text());
        return;
    }

    // ----------------------------------------------------------------
    // Заполняем таблицу
    // ----------------------------------------------------------------
    m_table->setRowCount(0);

    int row = 0;
    bool limitHit = false;
    const int lineHeight = m_table->fontMetrics().height() + 4;

    while (query.next()) {
        if (row >= kMaxRows) {
            limitHit = true;
            break;
        }

        const QString ts      = query.value(0).toDateTime()
                                     .toString("dd.MM.yyyy HH:mm:ss");
        const QString comp    = query.value(1).toString();
        const QString user    = query.value(2).toString();
        QString station       = query.value(3).toString();
        const QString op      = query.value(4).toString();
        const QString msg     = query.value(5).toString();

        // ✅ Пустая станция отображается как "—".
        if (station.isEmpty()) station = "—";

        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(ts));
        m_table->setItem(row, 1, new QTableWidgetItem(comp));
        m_table->setItem(row, 2, new QTableWidgetItem(user));
        m_table->setItem(row, 3, new QTableWidgetItem(station));
        m_table->setItem(row, 4, new QTableWidgetItem(op));
        m_table->setItem(row, 5, new QTableWidgetItem(msg));

        // ✅ Высота строки — по количеству '\n' в сообщении.
        const int lines = msg.count('\n') + 1;
        const int h = qMax(m_table->verticalHeader()->defaultSectionSize(),
                           lines * lineHeight);
        m_table->setRowHeight(row, h);

        ++row;
    }

    // ✅ Автопрокрутка вниз (как в MFC: если строк > 20).
    if (m_table->rowCount() > 20) {
        m_table->scrollToBottom();
    }

    // ✅ Сообщение о превышении лимита.
    if (limitHit) {
        QMessageBox::information(this, "Ограничение",
            QString("Показаны первые %1 записей.\n"
                    "Уточните диапазон дат или добавьте фильтры.")
                .arg(kMaxRows));
    }
}
