#include "driverconfigeditordialog.h"
#include "modulatordriver.h"
#include "alarmregistry.h"
#include "databasemanager.h"
#include "mainwindow.h"   // getCurrentUser() — не отсюда; используем DatabaseManager

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QListWidget>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QSplitter>
#include <QLabel>
#include <QFileDialog>
#include <QDebug>
#include <functional>

// ============================================================================
// ✅ КОНСТРУКТОР
// ============================================================================
DriverConfigEditorDialog::DriverConfigEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Редактор конфигов драйверов");
    resize(1200, 700);

    // -------------------- Левая панель: список файлов --------------------
    m_fileList = new QListWidget(this);
    m_fileList->setMinimumWidth(220);
    connect(m_fileList, &QListWidget::currentRowChanged,
            this, &DriverConfigEditorDialog::onFileSelected);

    // -------------------- Правая панель: вкладки --------------------
    m_tabs = new QTabWidget(this);

    // ===== Вкладка «Общее» =====
    QWidget *generalTab = new QWidget(this);
    QFormLayout *generalLayout = new QFormLayout(generalTab);

    m_modelEdit = new QLineEdit(generalTab);
    connect(m_modelEdit, &QLineEdit::textEdited, this, [this]() { m_dirty = true; });
    generalLayout->addRow("Модель:", m_modelEdit);

    QWidget *deviceIdsWidget = new QWidget(generalTab);
    QHBoxLayout *deviceIdsLayout = new QHBoxLayout(deviceIdsWidget);
    deviceIdsLayout->setContentsMargins(0, 0, 0, 0);
    m_deviceIdsList = new QListWidget(deviceIdsWidget);
    m_deviceIdsList->setMaximumHeight(120);
    deviceIdsLayout->addWidget(m_deviceIdsList, 1);

    QWidget *deviceIdControls = new QWidget(deviceIdsWidget);
    QVBoxLayout *deviceIdCtrlLayout = new QVBoxLayout(deviceIdControls);
    deviceIdCtrlLayout->setContentsMargins(0, 0, 0, 0);
    m_deviceIdSpin = new QSpinBox(deviceIdControls);
    m_deviceIdSpin->setRange(1, 999999);
    m_deviceIdSpin->setValue(1602);
    deviceIdCtrlLayout->addWidget(m_deviceIdSpin);
    m_addDeviceIdBtn = new QPushButton("+", deviceIdControls);
    connect(m_addDeviceIdBtn, &QPushButton::clicked,
            this, &DriverConfigEditorDialog::onAddDeviceId);
    deviceIdCtrlLayout->addWidget(m_addDeviceIdBtn);
    m_removeDeviceIdBtn = new QPushButton("-", deviceIdControls);
    connect(m_removeDeviceIdBtn, &QPushButton::clicked,
            this, &DriverConfigEditorDialog::onRemoveDeviceId);
    deviceIdCtrlLayout->addWidget(m_removeDeviceIdBtn);
    deviceIdCtrlLayout->addStretch();
    deviceIdsLayout->addWidget(deviceIdControls);

    generalLayout->addRow("deviceIds:", deviceIdsWidget);
    m_tabs->addTab(generalTab, "Общее");

    // ===== Вкладка «Параметры» =====
    QWidget *paramsTab = new QWidget(this);
    QVBoxLayout *paramsLayout = new QVBoxLayout(paramsTab);

    m_paramsTable = new QTableWidget(paramsTab);
    m_paramsTable->setColumnCount(12);
    m_paramsTable->setHorizontalHeaderLabels(QStringList()
        << "key" << "label" << "oid" << "type" << "target" << "scale"
        << "writable" << "commandCode" << "min" << "max" << "snmpType" << "oidTodo");
    // все Interactive, oid — Stretch
    for (int c = 0; c <= 11; ++c)
        m_paramsTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Interactive);
//    m_paramsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);   // oid
    m_paramsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_paramsTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                 | QAbstractItemView::EditKeyPressed);
    connect(m_paramsTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem*){
        if (!m_loading) { m_dirty = true; }
    });
    paramsLayout->addWidget(m_paramsTable);

    QHBoxLayout *paramsButtons = new QHBoxLayout();
    m_addParamBtn = new QPushButton("Добавить параметр", paramsTab);
    connect(m_addParamBtn, &QPushButton::clicked,
            this, &DriverConfigEditorDialog::onAddParam);
    paramsButtons->addWidget(m_addParamBtn);
    m_removeParamBtn = new QPushButton("Удалить", paramsTab);
    connect(m_removeParamBtn, &QPushButton::clicked,
            this, &DriverConfigEditorDialog::onRemoveParam);
    paramsButtons->addWidget(m_removeParamBtn);
    paramsButtons->addStretch();
    paramsLayout->addLayout(paramsButtons);

    m_tabs->addTab(paramsTab, "Параметры");

    // ===== Вкладка «Правила аварий» =====
    QWidget *rulesTab = new QWidget(this);
    QVBoxLayout *rulesLayout = new QVBoxLayout(rulesTab);

    m_rulesTable = new QTableWidget(rulesTab);
    m_rulesTable->setColumnCount(4);
    m_rulesTable->setHorizontalHeaderLabels(QStringList() << "field" << "op" << "value" << "code");
    m_rulesTable->horizontalHeader()->setStretchLastSection(true);
    m_rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(m_rulesTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem*){
        if (!m_loading) { m_dirty = true; }
    });
    rulesLayout->addWidget(m_rulesTable);

    QHBoxLayout *rulesButtons = new QHBoxLayout();
    m_addRuleBtn = new QPushButton("Добавить правило", rulesTab);
    connect(m_addRuleBtn, &QPushButton::clicked,
            this, &DriverConfigEditorDialog::onAddRule);
    rulesButtons->addWidget(m_addRuleBtn);
    m_removeRuleBtn = new QPushButton("Удалить", rulesTab);
    connect(m_removeRuleBtn, &QPushButton::clicked,
            this, &DriverConfigEditorDialog::onRemoveRule);
    rulesButtons->addWidget(m_removeRuleBtn);
    rulesButtons->addStretch();
    rulesLayout->addLayout(rulesButtons);

    m_tabs->addTab(rulesTab, "Правила аварий");

    // -------------------- Splitter --------------------
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_fileList);
    splitter->addWidget(m_tabs);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    // -------------------- Панель проблем --------------------
    QLabel *problemsLabel = new QLabel("Проблемы валидации:", this);
    problemsLabel->setStyleSheet("font-weight: bold; color: #333;");

    m_problems = new QTextEdit(this);
    m_problems->setReadOnly(true);
    m_problems->setMaximumHeight(120);
    m_problems->setStyleSheet("background-color: #FAFAFA;");

    // -------------------- Кнопки --------------------
    QHBoxLayout *bottomButtons = new QHBoxLayout();
    m_reloadBtn = new QPushButton("Перезагрузить", this);
    connect(m_reloadBtn, &QPushButton::clicked,
            this, &DriverConfigEditorDialog::onReload);
    bottomButtons->addWidget(m_reloadBtn);
    bottomButtons->addStretch();
    m_saveBtn = new QPushButton("Сохранить", this);
    m_saveBtn->setDefault(true);
    connect(m_saveBtn, &QPushButton::clicked,
            this, &DriverConfigEditorDialog::onSave);
    bottomButtons->addWidget(m_saveBtn);
    m_saveAsBtn = new QPushButton("Сохранить как...", this);
    connect(m_saveAsBtn, &QPushButton::clicked,
            this, &DriverConfigEditorDialog::onSaveAs);
    bottomButtons->addWidget(m_saveAsBtn);
    m_closeBtn = new QPushButton("Закрыть", this);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottomButtons->addWidget(m_closeBtn);

    // -------------------- Общая компоновка --------------------
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(splitter, 1);
    mainLayout->addWidget(problemsLabel);
    mainLayout->addWidget(m_problems);
    mainLayout->addLayout(bottomButtons);

    // -------------------- Первичная загрузка --------------------
    refreshFileList();

    // Если файлов нет — отключаем кнопки
    if (m_fileList->count() == 0) {
        m_problems->setText("Папка конфигов пуста или не существует.");
        updateUiFromModel();
    }
}

DriverConfigEditorDialog::~DriverConfigEditorDialog() = default;

// ============================================================================
// ✅ ПАПКА КОНФИГОВ
// ============================================================================
QString DriverConfigEditorDialog::configDir() const
{
    return QCoreApplication::applicationDirPath() + "/drivers/modulators";
}

// ============================================================================
// ✅ СПИСОК ФАЙЛОВ
// ============================================================================
void DriverConfigEditorDialog::refreshFileList()
{
    const QString currentPath = m_model.filePath;   // сохраняем выбор

    m_fileList->blockSignals(true);
    m_fileList->clear();

    QDir dir(configDir());
    if (dir.exists()) {
        const QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files);
        for (const QString &name : files) {
            QListWidgetItem *item = new QListWidgetItem(name, m_fileList);
            item->setData(Qt::UserRole, dir.filePath(name));
        }
    }
    m_fileList->blockSignals(false);

    // Восстановить выбор по пути
    if (!currentPath.isEmpty()) {
        for (int i = 0; i < m_fileList->count(); ++i) {
            if (m_fileList->item(i)->data(Qt::UserRole).toString() == currentPath) {
                m_fileList->setCurrentRow(i);
                break;
            }
        }
    }
}

// ============================================================================
// ✅ ВЫБОР ФАЙЛА
// ============================================================================
void DriverConfigEditorDialog::onFileSelected(int row)
{
    if (row < 0 || row >= m_fileList->count()) return;

    if (m_dirty) {
        const auto btn = QMessageBox::question(this, "Не сохранено",
            "Текущий конфиг изменён. Сохранить перед переходом?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (btn == QMessageBox::Cancel) {
            // Возвращаем выбор обратно
            m_fileList->blockSignals(true);
            for (int i = 0; i < m_fileList->count(); ++i) {
                if (m_fileList->item(i)->data(Qt::UserRole).toString() == m_model.filePath) {
                    m_fileList->setCurrentRow(i);
                    break;
                }
            }
            m_fileList->blockSignals(false);
            return;
        }
        if (btn == QMessageBox::Save) {
            if (!ensureSaved()) return;
        }
    }

    const QString path = m_fileList->item(row)->data(Qt::UserRole).toString();
    loadCurrentFile(path);
}

void DriverConfigEditorDialog::loadCurrentFile(const QString &path)
{
    QString err;
    if (!m_model.loadFromFile(path, &err)) {
        QMessageBox::warning(this, "Ошибка", QString("Не удалось загрузить:\n%1").arg(err));
        m_model = DriverConfigModel();
        m_model.filePath = path;
    }
    m_dirty = false;
    updateUiFromModel();
    runValidation();
}

// ============================================================================
// ✅ ОБНОВЛЕНИЕ UI ИЗ МОДЕЛИ
// ============================================================================
void DriverConfigEditorDialog::updateUiFromModel()
{
    m_loading = true;

    m_modelEdit->setText(m_model.model);
    refreshDeviceIds();
    refreshParamsTable();
    refreshRulesTable();

    m_loading = false;

    const bool hasFile = !m_model.filePath.isEmpty();
    m_saveBtn->setEnabled(hasFile);
    m_addParamBtn->setEnabled(hasFile);
    m_removeParamBtn->setEnabled(hasFile);
    m_addRuleBtn->setEnabled(hasFile);
    m_removeRuleBtn->setEnabled(hasFile);
    m_addDeviceIdBtn->setEnabled(hasFile);
    m_removeDeviceIdBtn->setEnabled(hasFile);
}

void DriverConfigEditorDialog::refreshDeviceIds()
{
    m_deviceIdsList->clear();
    for (int id : m_model.deviceIds) {
        m_deviceIdsList->addItem(QString::number(id));
    }
}

void DriverConfigEditorDialog::refreshParamsTable()
{
    m_loading = true;
    m_paramsTable->setRowCount(m_model.params.size());
    for (int i = 0; i < m_model.params.size(); ++i) {
        const auto &p = m_model.params[i];

        auto setText = [&](int col, const QString &text) {
            QTableWidgetItem *item = new QTableWidgetItem(text);
            m_paramsTable->setItem(i, col, item);
        };
        setText(0, p.key);
        setText(1, p.label);                       // ← НОВОЕ
        setText(2, p.oid);
        setText(3, DriverConfigModel::typeToString(p.type));
        setText(4, p.target);
        setText(5, QString::number(p.scale));
        // writable — checkbox
        QTableWidgetItem *w = new QTableWidgetItem();
        w->setFlags(w->flags() | Qt::ItemIsUserCheckable);
        w->setCheckState(p.writable ? Qt::Checked : Qt::Unchecked);
        m_paramsTable->setItem(i, 6, w);
        setText(7, p.commandCode == 0 ? QString() : QString::number(p.commandCode));
        setText(8, QString::number(p.min));
        setText(9, QString::number(p.max));
        setText(10, QString::number(p.snmpType));
        QTableWidgetItem *t = new QTableWidgetItem();
        t->setFlags(t->flags() | Qt::ItemIsUserCheckable);
        t->setCheckState(p.oidTodo ? Qt::Checked : Qt::Unchecked);
        m_paramsTable->setItem(i, 11, t);
    }
    m_loading = false;
}

void DriverConfigEditorDialog::refreshRulesTable()
{
    m_loading = true;
    m_rulesTable->setRowCount(m_model.alarmRules.size());
    for (int i = 0; i < m_model.alarmRules.size(); ++i) {
        const auto &r = m_model.alarmRules[i];
        m_rulesTable->setItem(i, 0, new QTableWidgetItem(r.field));
        m_rulesTable->setItem(i, 1, new QTableWidgetItem(DriverConfigModel::opToString(r.op)));
        m_rulesTable->setItem(i, 2, new QTableWidgetItem(QString::number(r.value)));
        m_rulesTable->setItem(i, 3, new QTableWidgetItem(QString::number(r.code)));
    }
    m_loading = false;
}

// ============================================================================
// ✅ СИНХРОНИЗАЦИЯ UI → МОДЕЛЬ
// ============================================================================
bool DriverConfigEditorDialog::syncUiToModel()
{
    m_model.model = m_modelEdit->text();

    // deviceIds
    m_model.deviceIds.clear();
    for (int i = 0; i < m_deviceIdsList->count(); ++i) {
        m_model.deviceIds.append(m_deviceIdsList->item(i)->text().toInt());
    }

    // params
    m_model.params.clear();
    for (int i = 0; i < m_paramsTable->rowCount(); ++i) {
        DriverConfigModel::Param p;
        auto getText = [&](int col) -> QString {
            auto *it = m_paramsTable->item(i, col);
            return it ? it->text() : QString();
        };
        p.key    = getText(0);
        p.oid    = getText(1);
        p.type   = DriverConfigModel::typeFromString(getText(2));
        p.target = getText(3);
        bool ok = true;
        p.scale  = getText(4).toDouble(&ok);
        if (!ok) p.scale = 1.0;
        auto *w = m_paramsTable->item(i, 5);
        p.writable = (w && w->checkState() == Qt::Checked);
        p.commandCode = getText(6).toInt();
        p.min = getText(7).toDouble();
        p.max = getText(8).toDouble();
        p.snmpType = getText(9).toInt();
        auto *t = m_paramsTable->item(i, 10);
        p.oidTodo = (t && t->checkState() == Qt::Checked);
        m_model.params.append(p);
    }

    // rules
    m_model.alarmRules.clear();
    for (int i = 0; i < m_rulesTable->rowCount(); ++i) {
        DriverConfigModel::Rule r;
        auto getText = [&](int col) -> QString {
            auto *it = m_rulesTable->item(i, col);
            return it ? it->text() : QString();
        };
        r.field = getText(0);
        r.op    = DriverConfigModel::opFromString(getText(1));
        r.value = getText(2).toDouble();
        r.code  = getText(3).toInt();
        m_model.alarmRules.append(r);
    }

    return true;
}

// ============================================================================
// ✅ ДЕЙСТВИЯ С deviceIds
// ============================================================================
void DriverConfigEditorDialog::onAddDeviceId()
{
    const int id = m_deviceIdSpin->value();
    for (int i = 0; i < m_deviceIdsList->count(); ++i) {
        if (m_deviceIdsList->item(i)->text().toInt() == id) return;   // уже есть
    }
    m_deviceIdsList->addItem(QString::number(id));
    m_dirty = true;
}

void DriverConfigEditorDialog::onRemoveDeviceId()
{
    const int row = m_deviceIdsList->currentRow();
    if (row < 0) return;
    delete m_deviceIdsList->takeItem(row);
    m_dirty = true;
}

// ============================================================================
// ✅ ДЕЙСТВИЯ С ПАРАМЕТРАМИ
// ============================================================================
void DriverConfigEditorDialog::onAddParam()
{
    const int row = m_paramsTable->rowCount();
    m_paramsTable->insertRow(row);
    m_paramsTable->setItem(row, 0, new QTableWidgetItem("newParam"));
    m_paramsTable->setItem(row, 1, new QTableWidgetItem(""));
    m_paramsTable->setItem(row, 2, new QTableWidgetItem("double"));
    m_paramsTable->setItem(row, 3, new QTableWidgetItem(""));
    m_paramsTable->setItem(row, 4, new QTableWidgetItem("1.0"));
    QTableWidgetItem *w = new QTableWidgetItem();
    w->setFlags(w->flags() | Qt::ItemIsUserCheckable);
    w->setCheckState(Qt::Unchecked);
    m_paramsTable->setItem(row, 5, w);
    m_paramsTable->setItem(row, 6, new QTableWidgetItem(""));
    m_paramsTable->setItem(row, 7, new QTableWidgetItem("0"));
    m_paramsTable->setItem(row, 8, new QTableWidgetItem("0"));
    m_paramsTable->setItem(row, 9, new QTableWidgetItem("2"));
    QTableWidgetItem *t = new QTableWidgetItem();
    t->setFlags(t->flags() | Qt::ItemIsUserCheckable);
    t->setCheckState(Qt::Checked);   // новый параметр — заглушка, помечаем oidTodo
    m_paramsTable->setItem(row, 10, t);
    m_dirty = true;
}

void DriverConfigEditorDialog::onRemoveParam()
{
    const int row = m_paramsTable->currentRow();
    if (row < 0) return;
    m_paramsTable->removeRow(row);
    m_dirty = true;
}

// ============================================================================
// ✅ ДЕЙСТВИЯ С ПРАВИЛАМИ
// ============================================================================
void DriverConfigEditorDialog::onAddRule()
{
    const int row = m_rulesTable->rowCount();
    m_rulesTable->insertRow(row);
    m_rulesTable->setItem(row, 0, new QTableWidgetItem("monitorRelayWarnInState"));
    m_rulesTable->setItem(row, 1, new QTableWidgetItem("!="));
    m_rulesTable->setItem(row, 2, new QTableWidgetItem("0"));
    m_rulesTable->setItem(row, 3, new QTableWidgetItem("1602"));
    m_dirty = true;
}

void DriverConfigEditorDialog::onRemoveRule()
{
    const int row = m_rulesTable->currentRow();
    if (row < 0) return;
    m_rulesTable->removeRow(row);
    m_dirty = true;
}

// ============================================================================
// ✅ ВАЛИДАЦИЯ
// ============================================================================
void DriverConfigEditorDialog::runValidation()
{
    if (!syncUiToModel()) {
        m_problems->setText("Ошибка синхронизации полей — проверьте числовые значения.");
        m_saveBtn->setEnabled(false);
        return;
    }

    const QStringList knownTargets = ModulatorDriver::knownTargets();
    const QStringList problems = m_model.validate(
        knownTargets,
        [](int code) { return AlarmRegistry::instance().hasCode(code); });

    if (problems.isEmpty()) {
        m_problems->setText("✅ Проблем не найдено.");
        m_problems->setStyleSheet("background-color: #E8F5E9; color: #1B5E20;");
        m_saveBtn->setEnabled(!m_model.filePath.isEmpty());
    } else {
        m_problems->setText("⚠ " + problems.join("\n⚠ "));
        m_problems->setStyleSheet("background-color: #FFF3E0; color: #E65100;");
        m_saveBtn->setEnabled(false);
    }
}

// ============================================================================
// ✅ СОХРАНЕНИЕ
// ============================================================================
bool DriverConfigEditorDialog::ensureSaved()
{
    if (!m_dirty) return true;
    if (m_model.filePath.isEmpty()) return true;
    QString err;
    if (!syncUiToModel()) return false;
    if (!m_model.saveToFile(m_model.filePath, &err)) {
        QMessageBox::critical(this, "Ошибка", QString("Не удалось сохранить:\n%1").arg(err));
        return false;
    }
    m_dirty = false;
    return true;
}

void DriverConfigEditorDialog::onSave()
{
    if (m_model.filePath.isEmpty()) {
        onSaveAs();
        return;
    }
    if (!syncUiToModel()) {
        QMessageBox::warning(this, "Ошибка", "Некорректные числовые значения в таблице.");
        return;
    }
    runValidation();
    if (m_problems->toPlainText().startsWith("⚠")) {
        const auto btn = QMessageBox::question(this, "Есть проблемы",
            "Валидация выявила проблемы. Сохранить всё равно?",
            QMessageBox::Save | QMessageBox::Cancel);
        if (btn == QMessageBox::Cancel) return;
    }

    QString err;
    if (!m_model.saveToFile(m_model.filePath, &err)) {
        QMessageBox::critical(this, "Ошибка", QString("Не удалось сохранить:\n%1").arg(err));
        return;
    }
    m_dirty = false;

    DatabaseManager::instance().logUserAction(
        "Драйверы",
        QFileInfo(m_model.filePath).fileName(),
        QString("Сохранён конфиг драйвера: model=%1, params=%2, rules=%3")
            .arg(m_model.model)
            .arg(m_model.params.size())
            .arg(m_model.alarmRules.size()));

    QMessageBox::information(this, "Готово", "Конфиг сохранён.");
}

void DriverConfigEditorDialog::onSaveAs()
{
    if (!syncUiToModel()) {
        QMessageBox::warning(this, "Ошибка", "Некорректные числовые значения в таблице.");
        return;
    }

    const QString startDir = configDir();
    QString path = QFileDialog::getSaveFileName(
        this, "Сохранить конфиг как",
        startDir.isEmpty() ? QString() : startDir,
        "JSON (*.json)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".json", Qt::CaseInsensitive)) path += ".json";

    QString err;
    if (!m_model.saveToFile(path, &err)) {
        QMessageBox::critical(this, "Ошибка", QString("Не удалось сохранить:\n%1").arg(err));
        return;
    }
    m_model.filePath = path;
    m_dirty = false;

    DatabaseManager::instance().logUserAction(
        "Драйверы",
        QFileInfo(path).fileName(),
        QString("Создан конфиг драйвера: model=%1").arg(m_model.model));

    refreshFileList();
    runValidation();
    QMessageBox::information(this, "Готово", "Конфиг сохранён как " + path);
}

void DriverConfigEditorDialog::onReload()
{
    if (m_model.filePath.isEmpty()) return;
    if (m_dirty) {
        const auto btn = QMessageBox::question(this, "Не сохранено",
            "Изменения будут потеряны. Продолжить?",
            QMessageBox::Yes | QMessageBox::No);
        if (btn != QMessageBox::Yes) return;
    }
    loadCurrentFile(m_model.filePath);
}
