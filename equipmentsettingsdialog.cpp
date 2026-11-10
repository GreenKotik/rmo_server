#include "equipmentsettingsdialog.h"
#include "basedocwidget.h"
#include "mainwindow.h"
#include "authrequireddialog.h"
#include "driverfactory.h"
#include "deviceids.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QMdiSubWindow>
#include <QDebug>

// ============================================================================
// КОНСТРУКТОР / ДЕСТРУКТОР
// ============================================================================

EquipmentSettingsDialog::EquipmentSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Настройка параметров");
    setMinimumSize(750, 650);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    tabWidget = new QTabWidget(this);
    tabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #D0D0D0; background: white; }"
        "QTabBar::tab { padding: 8px 16px; }"
        "QTabBar::tab:selected { background: #005A9E; color: white; }"
    );
    mainLayout->addWidget(tabWidget);

    createPollingTab();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton("Сохранить", this);
    okBtn->setDefault(true);
    okBtn->setStyleSheet(
        "QPushButton { background-color: #005A9E; color: white; padding: 8px 20px; "
        "border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #004A8E; }"
    );
    QPushButton *cancelBtn = new QPushButton("Отмена", this);
    cancelBtn->setStyleSheet(
        "QPushButton { padding: 8px 20px; border: 1px solid #B0B0B0; border-radius: 4px; }"
        "QPushButton:hover { background-color: #E0E0E0; }"
    );
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, this, &EquipmentSettingsDialog::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

EquipmentSettingsDialog::~EquipmentSettingsDialog() = default;

// ============================================================================
// УСТАНОВКА ПАРАМЕТРОВ
// ============================================================================

void EquipmentSettingsDialog::setDocumentFilePath(const QString &path)
{
    m_documentFilePath = path;
    setWindowTitle(QString("Настройка параметров: %1")
                   .arg(QFileInfo(path).completeBaseName()));
    loadSettings();
    createEquipmentTabs();
}

void EquipmentSettingsDialog::setCurrentUser(const QString &username)
{
    m_currentUser = username;
}

// ============================================================================
// ПОИСК ВИДЖЕТА ДОКУМЕНТА
// ============================================================================

BaseDocWidget* EquipmentSettingsDialog::findDocWidget() const
{
    MainWindow *mainWin = qobject_cast<MainWindow*>(parent());
    if (!mainWin) {
        qWarning() << "[EquipmentSettings] parent не является MainWindow!";
        return nullptr;
    }
    for (QMdiSubWindow *sw : mainWin->findChildren<QMdiSubWindow*>()) {
        if (BaseDocWidget *w = qobject_cast<BaseDocWidget*>(sw->widget())) {
            if (w->filePath() == m_documentFilePath) {
                return w;
            }
        }
    }
    qWarning() << "[EquipmentSettings] Виджет документа не найден:" << m_documentFilePath;
    return nullptr;
}

// ============================================================================
// ВКЛАДКА "ОПРОС"
// ============================================================================

void EquipmentSettingsDialog::createPollingTab()
{
    QWidget *pollingTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(pollingTab);

    QLabel *title = new QLabel("<b>Параметры опроса</b>", pollingTab);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    QFormLayout *formLayout = new QFormLayout();

    pollingIntervalSpin = new QSpinBox(pollingTab);
    pollingIntervalSpin->setRange(100, 60000);
    pollingIntervalSpin->setSuffix(" мс");
    pollingIntervalSpin->setValue(1000);
    pollingIntervalSpin->setSingleStep(100);
    pollingIntervalSpin->setStyleSheet(
        "QSpinBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
    );
    formLayout->addRow("Интервал опроса оборудования:", pollingIntervalSpin);

    dbWriteIntervalSpin = new QSpinBox(pollingTab);
    dbWriteIntervalSpin->setRange(1000, 3600000);
    dbWriteIntervalSpin->setSuffix(" мс");
    dbWriteIntervalSpin->setValue(5000);
    dbWriteIntervalSpin->setSingleStep(1000);
    dbWriteIntervalSpin->setStyleSheet(
        "QSpinBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
    );
    formLayout->addRow("Интервал записи в базу данных:", dbWriteIntervalSpin);

    // ✅ НОВОЕ: поле "Запись в базу при аварии в течении (минут)"
    alarmWriteDurationSpin = new QSpinBox(pollingTab);
    alarmWriteDurationSpin->setRange(0, 1440);           // 0..24 часа
    alarmWriteDurationSpin->setSuffix(" мин");
    alarmWriteDurationSpin->setValue(5);
    alarmWriteDurationSpin->setSingleStep(1);
    alarmWriteDurationSpin->setToolTip(
        "При появлении аварии запись в БД производится каждый цикл опроса\n"
        "в течение указанного времени. Если авария не исчезнет — запись\n"
        "продолжается только по таймеру.\n"
        "0 = отключить аварийную запись.");
    alarmWriteDurationSpin->setStyleSheet(
        "QSpinBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
    );
    formLayout->addRow("Запись в базу при аварии в течении (минут):",
                       alarmWriteDurationSpin);

    layout->addLayout(formLayout);

    QGroupBox *statusGroup = new QGroupBox("Статус оборудования", pollingTab);
    statusGroup->setStyleSheet(
        "QGroupBox { font-weight: bold; border: 1px solid #B0B0B0; "
        "border-radius: 4px; margin-top: 15px; padding-top: 20px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
    );
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);

    inWorkCheck = new QCheckBox("Оборудование находится в РАБОТЕ", pollingTab);
    inWorkCheck->setStyleSheet(
        "QCheckBox { font-size: 13px; font-weight: bold; padding: 6px; }"
        "QCheckBox::indicator { width: 20px; height: 20px; }"
    );
    inWorkCheck->setToolTip(
        "Если отмечено — заголовок документа будет ЗЕЛЁНЫМ (оборудование в работе).\n"
        "Если снято — заголовок будет СИНИМ (дежурный режим)."
    );
    statusLayout->addWidget(inWorkCheck);

    QLabel *hintLabel = new QLabel(
        "<i>💡 Зелёный заголовок = оборудование в работе<br>"
        "Синий заголовок = дежурный режим</i>",
        pollingTab
    );
    hintLabel->setStyleSheet("color: #666; font-size: 11px; margin-top: 5px;");
    hintLabel->setWordWrap(true);
    statusLayout->addWidget(hintLabel);

    layout->addWidget(statusGroup);
    layout->addStretch();
    tabWidget->addTab(pollingTab, "Опрос");
}

// ============================================================================
// ДИНАМИЧЕСКИЕ ВКЛАДКИ ДЛЯ ПОДКЛЮЧЕНИЙ
// ============================================================================

void EquipmentSettingsDialog::createEquipmentTabs()
{
    if (m_documentFilePath.isEmpty()) return;

    BaseDocWidget *doc = findDocWidget();
    if (!doc) return;

    QList<EquipmentConnection> connections = doc->getEquipmentConnections();

    for (int i = 0; i < connections.size(); ++i) {
        const EquipmentConnection &conn = connections[i];
        if (!conn.connected) continue;

        QString connectionName = conn.name.isEmpty()
            ? QString("Подключение %1").arg(i + 1)
            : conn.name;

        createConnectionTab(connectionName, conn);
    }
}

// ============================================================================
// ВКЛАДКА ПОДКЛЮЧЕНИЯ (ОБЩАЯ)
// ============================================================================

void EquipmentSettingsDialog::createConnectionTab(const QString &name,
                                                  const EquipmentConnection &conn)
{
    // ✅ ОСОБЫЙ СЛУЧАЙ: Датчик температуры Boreas 3
    if (conn.deviceId == ID_TEMP_SNS_BOREAS_3) {
        createTemperatureSensorTab(name, conn);
        return;
    }

    // --- Стандартная вкладка ---
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    QLabel *title = new QLabel(
        QString("<b>Настройки подключения: %1</b>").arg(name), tab);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 14px; margin: 10px;");
    layout->addWidget(title);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->addRow("Адрес:", new QLabel(conn.address.isEmpty() ? "—" : conn.address));
    formLayout->addRow("Оборудование:", new QLabel(conn.equipmentType.isEmpty() ? "—" : conn.equipmentType));
    formLayout->addRow("Интерфейс:", new QLabel(conn.connectionInterface));
    formLayout->addRow("IP-адрес:", new QLabel(conn.ipAddress.isEmpty() ? "—" : conn.ipAddress));
    formLayout->addRow("Порт:", new QLabel(QString::number(conn.port)));

    layout->addLayout(formLayout);
    layout->addStretch();
    tabWidget->addTab(tab, name);
}

// ============================================================================
// ✅ УПРОЩЕННАЯ ВКЛАДКА ДЛЯ ДАТЧИКА ТЕМПЕРАТУРЫ (4 ДАТЧИКА)
// ============================================================================

void EquipmentSettingsDialog::createTemperatureSensorTab(const QString &name,
                                                         const EquipmentConnection &conn)
{
    QWidget *tab = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(tab);

    // --- Шапка вкладки: название устройства + чек "Опрос состояния оборудования" ---
    QHBoxLayout *headerLayout = new QHBoxLayout();

    QString deviceName = conn.equipmentType.isEmpty()
        ? "Датчик температуры"
        : conn.equipmentType;

    QLabel *titleLabel = new QLabel(
        QString("<b style='font-size: 16px; color: #005A9E;'>%1</b>")
            .arg("Контроллер температуры "+deviceName), tab);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_tempPollingCheck = new QCheckBox("Опрос состояния оборудования", tab);
    m_tempPollingCheck->setChecked(true);
    m_tempPollingCheck->setStyleSheet(
        "QCheckBox { font-size: 12px; font-weight: bold; padding: 4px; }"
    );
    m_tempPollingCheck->setToolTip(
        "Если отмечено — опрос датчиков температуры выполняется.\n"
        "Если снято — опрос приостановлен."
    );
    headerLayout->addWidget(m_tempPollingCheck);

    mainLayout->addLayout(headerLayout);
    mainLayout->addSpacing(10);

    // --- Очищаем виджеты перед созданием ---
    m_temperatureWidgets.clear();
    m_temperatureWidgets.resize(4);

    // --- Создаем блок для каждого датчика ---
    for (int i = 0; i < 4; ++i) {
        QGroupBox *sensorGroup = new QGroupBox(
            QString("Датчик температуры №%1").arg(i + 1), tab);
        sensorGroup->setStyleSheet(
            "QGroupBox { font-weight: bold; border: 1px solid #B0B0B0; "
            "border-radius: 4px; margin-top: 12px; padding-top: 15px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; "
            "color: #005A9E; }"
        );

        QGridLayout *grid = new QGridLayout(sensorGroup);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(6);
        grid->setColumnStretch(1, 1); // Колонка с полем ввода растягивается
        grid->setColumnStretch(2, 0);
        grid->setColumnStretch(3, 1);

        TemperatureSensorWidgets &w = m_temperatureWidgets[i];

        // --- Строка 0: Название датчика ---
        QLabel *nameLabel = new QLabel("Название:", sensorGroup);
        w.nameEdit = new QLineEdit(sensorGroup);
        w.nameEdit->setPlaceholderText(QString("Датчик №%1").arg(i + 1));
        w.nameEdit->setStyleSheet(
            "QLineEdit { padding: 2px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        grid->addWidget(nameLabel, 0, 0);
        grid->addWidget(w.nameEdit, 0, 1, 1, 3); // Растягиваем на 3 колонки

        // --- Строка 1: Тип датчика + чек верхнего предела + значение ---
        QLabel *typeLabel = new QLabel("Тип датчика:", sensorGroup);
        w.typeCombo = new QComboBox(sensorGroup);
        w.typeCombo->addItems({"Нет", "Температура", "Температура и влажность"});
        w.typeCombo->setStyleSheet(
            "QComboBox { padding: 2px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QComboBox:focus { border: 1px solid #005A9E; }"
        );
        grid->addWidget(typeLabel, 1, 0);
        grid->addWidget(w.typeCombo, 1, 1);

        // Чек верхнего предела (справа)
        w.topPredelCheck = new QCheckBox("Верхний предел температуры:", sensorGroup);
        w.topPredelCheck->setToolTip("Включить контроль верхнего предела");
        grid->addWidget(w.topPredelCheck, 1, 2);

        // Значение верхнего предела (правее чека)
        w.topPredelSpin = new QDoubleSpinBox(sensorGroup);
        w.topPredelSpin->setRange(-100.0, 200.0);
        w.topPredelSpin->setDecimals(1);
        w.topPredelSpin->setSingleStep(0.1);
        w.topPredelSpin->setSuffix(" °C");
        w.topPredelSpin->setEnabled(false);
        w.topPredelSpin->setStyleSheet(
            "QDoubleSpinBox { padding: 2px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        grid->addWidget(w.topPredelSpin, 1, 3);

        connect(w.topPredelCheck, &QCheckBox::toggled,
                w.topPredelSpin, &QDoubleSpinBox::setEnabled);

        // --- Строка 2: Поправка температуры + чек нижнего предела + значение ---
        QLabel *popravkaLabel = new QLabel("Поправка температуры:", sensorGroup);
        w.popravkaSpin = new QDoubleSpinBox(sensorGroup);
        w.popravkaSpin->setRange(-100.0, 100.0);
        w.popravkaSpin->setDecimals(1);
        w.popravkaSpin->setSingleStep(0.1);
        w.popravkaSpin->setSuffix(" °C");
        w.popravkaSpin->setStyleSheet(
            "QDoubleSpinBox { padding: 2px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        grid->addWidget(popravkaLabel, 2, 0);
        grid->addWidget(w.popravkaSpin, 2, 1);

        // Чек нижнего предела (справа, под чеком верхнего предела)
        w.bottomPredelCheck = new QCheckBox("Нижний предел температуры:", sensorGroup);
        w.bottomPredelCheck->setToolTip("Включить контроль нижнего предела");
        grid->addWidget(w.bottomPredelCheck, 2, 2);

        // Значение нижнего предела (правее чека, под значением верхнего)
        w.bottomPredelSpin = new QDoubleSpinBox(sensorGroup);
        w.bottomPredelSpin->setRange(-100.0, 200.0);
        w.bottomPredelSpin->setDecimals(1);
        w.bottomPredelSpin->setSingleStep(0.1);
        w.bottomPredelSpin->setSuffix(" °C");
        w.bottomPredelSpin->setEnabled(false);
        w.bottomPredelSpin->setStyleSheet(
            "QDoubleSpinBox { padding: 2px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        grid->addWidget(w.bottomPredelSpin, 2, 3);

        connect(w.bottomPredelCheck, &QCheckBox::toggled,
                w.bottomPredelSpin, &QDoubleSpinBox::setEnabled);

        mainLayout->addWidget(sensorGroup);
    }

    mainLayout->addStretch();
    tabWidget->addTab(tab, name);

    // ✅ Загружаем ранее сохраненные настройки
    loadTemperatureSettings();
}

// ============================================================================
// ЗАГРУЗКА НАСТРОЕК
// ============================================================================

void EquipmentSettingsDialog::loadSettings()
{
    if (m_documentFilePath.isEmpty()) return;

    BaseDocWidget *doc = findDocWidget();
    if (!doc) return;

    QJsonObject settings = doc->getSettings();
    pollingIntervalSpin->setValue(settings.value("polling_interval").toInt(1000));
    dbWriteIntervalSpin->setValue(settings.value("db_write_interval").toInt(5000));
    inWorkCheck->setChecked(settings.value("in_work").toBool(false));

    // ✅ НОВОЕ: длительность аварийной записи
    alarmWriteDurationSpin->setValue(
        settings.value("alarm_write_duration_min").toInt(5));
}

// ✅ Загрузка настроек датчиков температуры
void EquipmentSettingsDialog::loadTemperatureSettings()
{
    if (m_documentFilePath.isEmpty()) return;

    QFile file(m_documentFilePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) return;

    QJsonDocument docJson = QJsonDocument::fromJson(file.readAll());
    QJsonObject obj = docJson.object();
    file.close();

    // Чек "Опрос состояния оборудования"
    if (m_tempPollingCheck) {
        m_tempPollingCheck->setChecked(
            obj.value("temperature_polling_enabled").toBool(true));
    }

    QJsonArray sensorsArray = obj.value("temperature_sensors").toArray();

    for (int i = 0; i < 4 && i < sensorsArray.size() && i < m_temperatureWidgets.size(); ++i) {
        QJsonObject sensorObj = sensorsArray[i].toObject();
        TemperatureSensorWidgets &w = m_temperatureWidgets[i];

        if (w.nameEdit)
            w.nameEdit->setText(sensorObj.value("name").toString(QString("Датчик №%1").arg(i + 1)));

        // ✅ Тип датчика
        if (w.typeCombo) {
            QString type = sensorObj.value("type").toString("Нет");
            int idx = w.typeCombo->findText(type);
            if (idx >= 0) w.typeCombo->setCurrentIndex(idx);
            else w.typeCombo->setCurrentIndex(0);
        }

        if (w.popravkaSpin)
            w.popravkaSpin->setValue(sensorObj.value("popravka").toDouble(0.0));

        if (w.topPredelCheck) {
            bool enabled = sensorObj.value("topPredelEnabled").toBool(false);
            w.topPredelCheck->setChecked(enabled);
            if (w.topPredelSpin) {
                w.topPredelSpin->setValue(sensorObj.value("topPredel").toDouble(0.0));
                w.topPredelSpin->setEnabled(enabled);
            }
        }

        if (w.bottomPredelCheck) {
            bool enabled = sensorObj.value("bottomPredelEnabled").toBool(false);
            w.bottomPredelCheck->setChecked(enabled);
            if (w.bottomPredelSpin) {
                w.bottomPredelSpin->setValue(sensorObj.value("bottomPredel").toDouble(0.0));
                w.bottomPredelSpin->setEnabled(enabled);
            }
        }
    }
}

// ============================================================================
// СОХРАНЕНИЕ
// ============================================================================

void EquipmentSettingsDialog::onSave()
{
    // ✅ Проверка авторизации
    if (m_currentUser.isEmpty()) {
        AuthRequiredDialog authDlg(this);
        if (authDlg.exec() != QDialog::Accepted) {
            return;
        }
        m_currentUser = authDlg.authenticatedUser();

        MainWindow *mainWin = qobject_cast<MainWindow*>(parent());
        if (mainWin) {
            mainWin->forceAuthenticate(m_currentUser);
        }
    }

    if (m_documentFilePath.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "Не указан файл документа.");
        return;
    }

    BaseDocWidget *doc = findDocWidget();
    if (!doc) {
        QMessageBox::critical(this, "Ошибка", "Не удалось найти виджет документа.");
        return;
    }

    // ========================================================================
    // 1. Читаем существующий файл
    // ========================================================================
    QFile file(m_documentFilePath);
    QJsonObject obj;
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument docJson = QJsonDocument::fromJson(file.readAll());
        obj = docJson.object();
        file.close();
    }

    // ========================================================================
    // 2. Сохраняем секцию "settings"
    // ========================================================================
    QJsonObject settings;
    settings["polling_interval"] = pollingIntervalSpin->value();
    settings["db_write_interval"] = dbWriteIntervalSpin->value();
    settings["in_work"] = inWorkCheck->isChecked();
    // ✅ НОВОЕ: длительность аварийной записи
    settings["alarm_write_duration_min"] = alarmWriteDurationSpin->value();
    obj["settings"] = settings;

    // ========================================================================
    // 3. Чек "Опрос состояния оборудования"
    // ========================================================================
    if (m_tempPollingCheck) {
        obj["temperature_polling_enabled"] = m_tempPollingCheck->isChecked();
    }

    // ========================================================================
    // 4. Сохраняем настройки датчиков температуры
    // ========================================================================
    if (!m_temperatureWidgets.isEmpty()) {
        QJsonArray sensorsArray;
        for (int i = 0; i < m_temperatureWidgets.size(); ++i) {
            const TemperatureSensorWidgets &w = m_temperatureWidgets[i];
            QJsonObject sensorObj;

            if (w.nameEdit) sensorObj["name"] = w.nameEdit->text();
            if (w.typeCombo) sensorObj["type"] = w.typeCombo->currentText();
            if (w.popravkaSpin) sensorObj["popravka"] = w.popravkaSpin->value();

            if (w.topPredelCheck) {
                sensorObj["topPredelEnabled"] = w.topPredelCheck->isChecked();
                if (w.topPredelSpin) sensorObj["topPredel"] = w.topPredelSpin->value();
            }

            if (w.bottomPredelCheck) {
                sensorObj["bottomPredelEnabled"] = w.bottomPredelCheck->isChecked();
                if (w.bottomPredelSpin) sensorObj["bottomPredel"] = w.bottomPredelSpin->value();
            }

            sensorsArray.append(sensorObj);
        }
        obj["temperature_sensors"] = sensorsArray;
    }

    // ========================================================================
    // ✅ 5. ЗАПИСЫВАЕМ ФАЙЛ (ЭТОГО НЕ ХВАТАЛО!)
    // ========================================================================
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, "Ошибка",
            QString("Не удалось открыть файл для записи:\n%1\n\n%2")
                .arg(m_documentFilePath, file.errorString()));
        return;
    }

    QByteArray jsonData = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    qint64 written = file.write(jsonData);
    file.close();

    if (written != jsonData.size()) {
        QMessageBox::critical(this, "Ошибка",
            "Не удалось полностью записать данные в файл.");
        return;
    }

    qDebug() << "[EquipmentSettings] Настройки сохранены в" << m_documentFilePath;

    // ========================================================================
    // 6. Обновляем состояние "в работе" в виджете документа
    // ========================================================================
    doc->setInWork(inWorkCheck->isChecked());

    // ========================================================================
    // 7. Обновляем цвет заголовка
    // ========================================================================
    bool hasAnyError = false;
    for (const EquipmentConnection &conn : doc->getEquipmentConnections()) {
        if (conn.errorCode != 0) {
            hasAnyError = true;
            break;
        }
    }

    if (hasAnyError) {
        doc->setEquipmentState(2); // Оранжевый (авария)
    } else if (inWorkCheck->isChecked()) {
        doc->setEquipmentState(1); // Зелёный (в работе)
    } else {
        doc->setEquipmentState(0); // Синий (дежурный)
    }

    // ========================================================================
    // 8. Перезагружаем настройки опроса в виджете документа
    // ========================================================================
    doc->reloadPollingSettings();

    // ========================================================================
    // 9. Уведомляем пользователя и закрываем диалог
    // ========================================================================
    QMessageBox::information(this, "Успех",
        QString("Настройки сохранены.\nАвторизован как: %1").arg(m_currentUser));
    accept();
}
