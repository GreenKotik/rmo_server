#ifndef EQUIPMENTSETTINGSDIALOG_H
#define EQUIPMENTSETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QMessageBox>
#include <QDebug>
#include <QFileInfo>
#include <QMdiSubWindow>
#include <QCheckBox>
#include <QGroupBox>
#include "mainwindow.h"
#include "authrequireddialog.h"

class EquipmentSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EquipmentSettingsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Настройка параметров");
        setMinimumSize(600, 450);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        // ✅ QTabWidget для закладок
        tabWidget = new QTabWidget(this);
        mainLayout->addWidget(tabWidget);

        // ✅ Закладка "Опрос" — всегда есть
        createPollingTab();

        // Кнопки
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

    // ✅ НОВЫЙ МЕТОД: Установить путь к файлу документа
    void setDocumentFilePath(const QString &path)
    {
        m_documentFilePath = path;
        setWindowTitle(QString("Настройка параметров: %1").arg(QFileInfo(path).completeBaseName()));
        loadSettings();
        createEquipmentTabs(); // ✅ Создаём динамические закладки после загрузки пути
    }

    // ✅ МЕТОД: Установить текущего пользователя
    void setCurrentUser(const QString &username)
    {
        m_currentUser = username;
    }

    // ✅ НОВЫЙ МЕТОД: Найти виджет документа по пути к файлу
    BaseDocWidget* findDocWidget() const
    {
        MainWindow *mainWin = qobject_cast<MainWindow*>(parent());
        if (!mainWin) {
            qWarning() << "[EquipmentSettings] ВНИМАНИЕ: parent не является MainWindow!";
            return nullptr;
        }
        for (QMdiSubWindow *sw : mainWin->findChildren<QMdiSubWindow*>()) {
            if (BaseDocWidget *w = qobject_cast<BaseDocWidget*>(sw->widget())) {
                if (w->filePath() == m_documentFilePath) {
                    return w;
                }
            }
        }
        qWarning() << "[EquipmentSettings] Виджет документа не найден для пути:" << m_documentFilePath;
        return nullptr;
    }

    // ✅ Создаёт динамические закладки только для подключённых устройств
    void createEquipmentTabs()
    {
        if (m_documentFilePath.isEmpty()) return;

        // ✅ ПОЛУЧАЕМ ДАННЫЕ ИЗ ВИДЖЕТА ДОКУМЕНТА (а не из файла!)
        BaseDocWidget *doc = findDocWidget();
        if (!doc) {
            qWarning() << "[EquipmentSettings] Не удалось получить данные из виджета документа";
            return;
        }

        QList<EquipmentConnection> connections = doc->getEquipmentConnections();
        qDebug() << "[EquipmentSettings] Получено подключений из виджета:" << connections.size();

        int createdTabs = 0;
        int skippedTabs = 0;

        // ✅ Для каждого подключения проверяем флаг "connected"
        for (int i = 0; i < connections.size(); ++i) {
            const EquipmentConnection &conn = connections[i];

            // ✅ Проверяем, подключено ли устройство
            if (!conn.connected) {
                skippedTabs++;
                qDebug() << "[EquipmentSettings] Пропущено неподключённое устройство:" << conn.name;
                continue;
            }

            QString connectionName = conn.name;
            if (connectionName.isEmpty()) {
                connectionName = QString("Подключение %1").arg(i + 1);
            }

            // ✅ Передаём структуру EquipmentConnection вместо QJsonObject
            createConnectionTab(connectionName, conn);
            createdTabs++;
        }

        qDebug() << "[EquipmentSettings] Создано закладок:" << createdTabs
                 << ", пропущено:" << skippedTabs;
    }

private:
    QTabWidget *tabWidget;
    QSpinBox *pollingIntervalSpin;
    QSpinBox *dbWriteIntervalSpin;
    QCheckBox *inWorkCheck;  // ✅ НОВОЕ: "Оборудование в работе"
    QString m_documentFilePath;
    QString m_currentUser;

    // ✅ Создаёт закладку "Опрос"
    void createPollingTab()
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

        layout->addLayout(formLayout);

        // ✅ НОВЫЙ CheckBox "Оборудование находится в РАБОТЕ"
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

    // ✅ НОВОЕ: Создаёт закладку для конкретного подключения (принимает EquipmentConnection)
    void createConnectionTab(const QString &name, const EquipmentConnection &conn)
    {
        QWidget *tab = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(tab);

        // Заголовок вкладки
        QLabel *title = new QLabel(
            QString("<b>Настройки подключения: %1</b>").arg(name), tab);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("font-size: 14px; margin: 10px;");
        layout->addWidget(title);

        // Информация о подключении
        QFormLayout *formLayout = new QFormLayout();

        // Адрес
        QLabel *addressLabel = new QLabel(
            conn.address.isEmpty() ? "не указан" : conn.address, tab);
        addressLabel->setStyleSheet("color: #005A9E; font-weight: bold;");
        formLayout->addRow("Адрес:", addressLabel);

        // Тип оборудования
        QLabel *equipmentLabel = new QLabel(
            conn.equipmentType.isEmpty() ? "не выбран" : conn.equipmentType, tab);
        equipmentLabel->setStyleSheet("color: #005A9E; font-weight: bold;");
        formLayout->addRow("Оборудование:", equipmentLabel);

        // Интерфейс
        QLabel *interfaceLabel = new QLabel(conn.interface, tab);
        interfaceLabel->setStyleSheet("color: #005A9E; font-weight: bold;");
        formLayout->addRow("Интерфейс:", interfaceLabel);

        // IP-адрес
        QLabel *ipLabel = new QLabel(
            conn.ipAddress.isEmpty() ? "не указан" : conn.ipAddress, tab);
        ipLabel->setStyleSheet("color: #005A9E; font-weight: bold;");
        formLayout->addRow("IP-адрес:", ipLabel);

        // Порт
        QLabel *portLabel = new QLabel(QString::number(conn.port), tab);
        portLabel->setStyleSheet("color: #005A9E; font-weight: bold;");
        formLayout->addRow("Порт:", portLabel);

        // ✅ Статус подключения + код ошибки (если есть)
        QString statusText;
        QString statusStyle;
        if (conn.errorCode != 0) {
            statusText = QString("⚠️ Ошибка (код: %1)").arg(conn.errorCode);
            statusStyle = "color: #E65100; font-weight: bold; font-size: 13px;"; // Оранжевый
        } else if (conn.connected) {
            statusText = "✅ Подключено";
            statusStyle = "color: #2E7D32; font-weight: bold; font-size: 13px;";
        } else {
            statusText = "❌ Отключено";
            statusStyle = "color: #C62828; font-weight: bold; font-size: 13px;";
        }
        QLabel *statusLabel = new QLabel(statusText, tab);
        statusLabel->setStyleSheet(statusStyle);
        formLayout->addRow("Статус:", statusLabel);

        // Поток
        QLabel *streamLabel = new QLabel(conn.useStream ? "Да" : "Нет", tab);
        formLayout->addRow("Поток:", streamLabel);

        layout->addLayout(formLayout);

        // Подсказка
        QLabel *hintLabel = new QLabel(
            "<i>Детальные параметры этого подключения будут добавлены позже.</i>", tab);
        hintLabel->setAlignment(Qt::AlignCenter);
        hintLabel->setStyleSheet("color: #666; font-size: 11px; margin-top: 20px;");
        layout->addWidget(hintLabel);

        layout->addStretch();

        // ✅ Добавляем закладку с именем подключения
        tabWidget->addTab(tab, name);
    }

    // ✅ Загружает настройки из виджета документа
    void loadSettings()
    {
        if (m_documentFilePath.isEmpty()) return;
        BaseDocWidget *doc = findDocWidget();
        if (!doc) {
            qWarning() << "[EquipmentSettings] Не удалось получить настройки из виджета";
            return;
        }

        // ✅ Получаем секцию settings напрямую из виджета
        QJsonObject settings = doc->getSettings();
        pollingIntervalSpin->setValue(settings.value("polling_interval").toInt(1000));
        dbWriteIntervalSpin->setValue(settings.value("db_write_interval").toInt(5000));

        // ✅ НОВОЕ: загружаем состояние "в работе"
        inWorkCheck->setChecked(settings.value("in_work").toBool(false));

        qDebug() << "[EquipmentSettings] Загружены настройки из виджета документа:" << m_documentFilePath;
    }

private slots:
void onSave()
{
    // ✅ ПРОВЕРКА АВТОРИЗАЦИИ — если не авторизован, показываем диалог
    if (m_currentUser.isEmpty()) {
        AuthRequiredDialog authDlg(this);
        if (authDlg.exec() != QDialog::Accepted) {
            // Пользователь отменил авторизацию
            return;
        }

        // ✅ Авторизация успешна — сохраняем имя пользователя
        m_currentUser = authDlg.authenticatedUser();

        // ✅ СИНХРОНИЗИРУЕМ С MAINWINDOW (чтобы таймер и права обновились)
        MainWindow *mainWin = qobject_cast<MainWindow*>(parent());
        if (mainWin) {
            mainWin->forceAuthenticate(m_currentUser);
        }

        qDebug() << "[EquipmentSettings] Пользователь авторизован через диалог:" << m_currentUser;
    }

    if (m_documentFilePath.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "Не указан файл документа.");
        return;
    }

    // ✅ Находим виджет документа
    BaseDocWidget *doc = findDocWidget();
    if (!doc) {
        QMessageBox::critical(this, "Ошибка", "Не удалось найти виджет документа.");
        return;
    }

    // ✅ Читаем существующий файл документа
    QFile file(m_documentFilePath);
    QJsonObject obj;
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument docJson = QJsonDocument::fromJson(file.readAll());
        obj = docJson.object();
        file.close();
    }

    // ✅ Обновляем секцию "settings"
    QJsonObject settings;
    settings["polling_interval"] = pollingIntervalSpin->value();
    settings["db_write_interval"] = dbWriteIntervalSpin->value();
    settings["in_work"] = inWorkCheck->isChecked(); // ✅ Сохраняем состояние
    obj["settings"] = settings;

    // ✅ Сохраняем обратно в файл документа
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson());
        file.close();

        // ✅ Обновляем состояние "в работе" в виджете
        doc->setInWork(inWorkCheck->isChecked());

        // ✅ УСТАНАВЛИВАЕМ ЦВЕТ ЗАГОЛОВКА
        // ✅ Проверяем наличие ошибок в подключениях (а не текущее состояние!)
        bool hasAnyError = false;
        for (const EquipmentConnection &conn : doc->getEquipmentConnections()) {
            if (conn.errorCode != 0) {
                hasAnyError = true;
                break;
            }
        }
        if (hasAnyError) {
            // ✅ Если есть ошибка — ОРАНЖЕВЫЙ (авария имеет приоритет)
            doc->setEquipmentState(2);
            qDebug() << "[EquipmentSettings] Заголовок: ОРАНЖЕВЫЙ (авария)";
        } else if (inWorkCheck->isChecked()) {
            // ✅ Нет ошибки + оборудование в работе → ЗЕЛЁНЫЙ
            doc->setEquipmentState(1);
            qDebug() << "[EquipmentSettings] Заголовок: ЗЕЛЁНЫЙ (в работе)";
        } else {
            // ✅ Нет ошибки + не в работе → СИНИЙ
            doc->setEquipmentState(0);
            qDebug() << "[EquipmentSettings] Заголовок: СИНИЙ (дежурный)";
        }

        QMessageBox::information(this, "Успех",
            QString("Настройки сохранены в файл:\n%1\nАвторизован как: %2")
                .arg(m_documentFilePath, m_currentUser));
        accept();
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить настройки.");
    }
    // ✅ НОВОЕ: Перезагружаем настройки опроса в виджете документа
    if (doc) {
        doc->reloadPollingSettings();
        qDebug() << "[EquipmentSettings] Настройки опроса перезагружены для:" << m_documentFilePath;
    }
}

};

#endif // EQUIPMENTSETTINGSDIALOG_H
