#ifndef EQUIPMENTCONFIGDIALOG_H
#define EQUIPMENTCONFIGDIALOG_H

#include "connectionparamsdialog.h"

#include <QDialog>
#include <QTableWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QHeaderView>
#include "mainwindow.h" // Для EquipmentConnection
#include <QColor>
#include <QStyle>

class EquipmentConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EquipmentConfigDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Конфигурация оборудования");
        setMinimumSize(900, 800);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(12);
        mainLayout->setContentsMargins(20, 20, 20, 20);

        // Заголовок
        QLabel *headerLabel = new QLabel(
            "<b>Конфигурация подключений оборудования</b><br>"
            "Настройте параметры подключения для каждого устройства", this);
        headerLabel->setWordWrap(true);
        mainLayout->addWidget(headerLabel);

        // ✅ Таблица с элементами управления
        equipmentTable = new QTableWidget(this);
        equipmentTable->setColumnCount(7);
        equipmentTable->setHorizontalHeaderLabels({
            "Название", "Подключено", "Адрес", "Оборудование", "Поток", "Медленное", "Соединение"
        });
        equipmentTable->horizontalHeader()->setStretchLastSection(false);
        // Столбец 0: Название — расширен на 20% (было 200, стало 240)
        equipmentTable->setColumnWidth(0, 280);
        // Столбец 1: Подключено
        equipmentTable->setColumnWidth(1, 90);
        // Столбец 2: Адрес
        equipmentTable->setColumnWidth(2, 70);
        // Столбец 3: Оборудование — растягивается
        equipmentTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        // Столбец 4: Поток
        equipmentTable->setColumnWidth(4, 70);
        // Столбец 5: Медленное (новый)
        equipmentTable->setColumnWidth(5, 90);  // ✅ НОВЫЙ СТОЛБЕЦ
        // Столбец 6: Параметры (был 5-й)
        equipmentTable->setColumnWidth(6, 120);
        equipmentTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        equipmentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        equipmentTable->setAlternatingRowColors(true);
        equipmentTable->verticalHeader()->setDefaultSectionSize(40);
        equipmentTable->setStyleSheet(
            "QTableWidget { gridline-color: #D0D0D0; }"
            "QHeaderView::section { "
            "  background-color: #005A9E; color: white; "
            "  padding: 6px; font-weight: bold; "
            "}"
        );
        mainLayout->addWidget(equipmentTable);

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

        connect(okBtn, &QPushButton::clicked, this, &EquipmentConfigDialog::onSave);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    }

    // ✅ Установить путь к файлу документа
    void setDocumentFilePath(const QString &path)
    {
        m_documentFilePath = path;
        // ✅ Убираем расширение через completeBaseName()
        setWindowTitle(QString("Конфигурация оборудования: %1").arg(QFileInfo(path).completeBaseName()));
        loadConnections();
    }

    // ✅ Установить тип документа
    void setDocumentType(DocType type)
    {
        m_docType = type;
    }

    // ✅ Установить текущего пользователя
    void setCurrentUser(const QString &username)
    {
        m_currentUser = username;
    }

private:
    QTableWidget *equipmentTable;
    QString m_documentFilePath;
    DocType m_docType;
    QString m_currentUser;
    QList<EquipmentConnection> m_connections;

    // ✅ Загрузить подключения из виджета документа (а не из файла напрямую)
    void loadConnections()
    {
        qDebug() << "[EquipmentConfig] Загрузка из виджета документа:" << m_documentFilePath;

        // Находим виджет документа через MDI-область главного окна
        MainWindow *mainWin = qobject_cast<MainWindow*>(parent());
        BaseDocWidget *doc = nullptr;
        if (mainWin) {
            for (QMdiSubWindow *sw : mainWin->findChildren<QMdiSubWindow*>()) {
                if (BaseDocWidget *w = qobject_cast<BaseDocWidget*>(sw->widget())) {
                    if (w->filePath() == m_documentFilePath) {
                        doc = w;
                        break;
                    }
                }
            }
        }

        if (!doc) {
            qWarning() << "[EquipmentConfig] ВНИМАНИЕ: Виджет документа не найден!";
            return;
        }

        // ✅ Получаем массив подключений из виджета документа
        m_connections = doc->getEquipmentConnections();

        qDebug() << "[EquipmentConfig] Загружено подключений из виджета:" << m_connections.size();
        createTable();
    }


    // ✅ Создать таблицу с элементами управления
    void createTable()
    {
        equipmentTable->setRowCount(m_connections.size());
        for (int i = 0; i < m_connections.size(); ++i) {
            const EquipmentConnection &conn = m_connections[i];

            // ✅ Определяем цвет фона строки на основе errorCode
            bool hasError = (conn.errorCode != 0);
            QColor rowColor = hasError ? QColor("#FFF59D") : QColor(Qt::white); // ✅ Жёлтый или белый

            // Столбец 0: Название (только текст)
            QTableWidgetItem *nameItem = new QTableWidgetItem(conn.name);
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
            nameItem->setBackground(rowColor); // ✅ Устанавливаем фон ячейки
            equipmentTable->setItem(i, 0, nameItem);

            // ✅ Если есть ошибка — добавляем иконку предупреждения и подсказку
            if (hasError) {
                QString issue;
                switch (conn.errorCode) {
                    case 1:
                        issue = "не совпадает индекс";
                        break;
                    case 2:
                        issue = "не совпадает имя";
                        break;
                    case 3:
                        issue = "не совпадают индекс и имя";
                        break;
                    case 4:
                        issue = "строка отсутствует в файле конфигурации";
                        break;
                    default:
                        issue = QString("неизвестная ошибка (код: %1)").arg(conn.errorCode);
                        break;
                }

                nameItem->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
                nameItem->setToolTip(
                    QString("⚠️ Обнаружена ошибка конфигурации (код: %1).\n"
                            "Проблема: %2\n"
                            "Проверьте целостность файла документа.")
                        .arg(conn.errorCode)
                        .arg(issue)
                );
            }
            // Столбец 1: Подключено (CheckBox)
            QCheckBox *connectedCheck = new QCheckBox();
            connectedCheck->setChecked(conn.connected);

            // ✅ Контейнер для CheckBox с фоном строки
            QWidget *checkContainer = new QWidget();
            checkContainer->setStyleSheet(
                QString("background-color: %1;").arg(rowColor.name())
            );
            QHBoxLayout *checkLayout = new QHBoxLayout(checkContainer);
            checkLayout->setContentsMargins(0, 0, 0, 0);
            checkLayout->addWidget(connectedCheck);
            checkLayout->setAlignment(connectedCheck, Qt::AlignCenter);
            equipmentTable->setCellWidget(i, 1, checkContainer);

            // Столбец 2: Адрес (LineEdit)
            QLineEdit *addressEdit = new QLineEdit();
            addressEdit->setText(conn.address);
            addressEdit->setPlaceholderText("0");
            addressEdit->setStyleSheet(
                QString("QLineEdit { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; "
                        "background-color: %1; }").arg(rowColor.name()) +
                "QLineEdit:focus { border: 1px solid #005A9E; }"
            );
            equipmentTable->setCellWidget(i, 2, addressEdit);

            // ✅ Столбец 3: Оборудование (ComboBox с кодами устройств)
            QComboBox *equipmentCombo = new QComboBox();
            MainWindow *mainWin = qobject_cast<MainWindow*>(parent());

            QList<QPair<QString, int>> equipmentList;
            if (mainWin) {
                equipmentList = mainWin->getEquipmentListForRow(m_docType, i);
            } else {
                equipmentList << qMakePair(QString("Устройство 1"), ID_UNKNOWN);
                equipmentList << qMakePair(QString("Устройство 2"), ID_UNKNOWN);
                qWarning() << "[EquipmentConfig] ВНИМАНИЕ: mainWin = nullptr! Используется fallback";
            }

            // ✅ Заполняем ComboBox парами (название, код)
            for (const auto &pair : equipmentList) {
                equipmentCombo->addItem(pair.first, pair.second);  // ✅ second = deviceId
            }

            // ✅ Устанавливаем текущее значение по сохранённому deviceId
            if (conn.deviceId != ID_UNKNOWN) {
                int index = equipmentCombo->findData(conn.deviceId);
                if (index >= 0) {
                    equipmentCombo->setCurrentIndex(index);
                } else {
                    // Код не найден в списке — пытаемся по имени
                    equipmentCombo->setCurrentText(conn.equipmentType);
                }
            } else {
                // Код не сохранён — устанавливаем по имени
                equipmentCombo->setCurrentText(conn.equipmentType);
            }

            equipmentCombo->setStyleSheet(
                QString("QComboBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; "
                        "background-color: %1; }").arg(rowColor.name()) +
                "QComboBox:focus { border: 1px solid #005A9E; }"
            );
            equipmentTable->setCellWidget(i, 3, equipmentCombo);

            // Столбец 4: Поток (CheckBox)
            QCheckBox *streamCheck = new QCheckBox();
            streamCheck->setChecked(conn.useStream);
            // ✅ Контейнер для CheckBox с фоном строки
            QWidget *streamContainer = new QWidget();
            streamContainer->setStyleSheet(
                QString("background-color: %1;").arg(rowColor.name())
            );
            QHBoxLayout *streamLayout = new QHBoxLayout(streamContainer);
            streamLayout->setContentsMargins(0, 0, 0, 0);
            streamLayout->addWidget(streamCheck);
            streamLayout->setAlignment(streamCheck, Qt::AlignCenter);
            equipmentTable->setCellWidget(i, 4, streamContainer);

            // ✅ НОВЫЙ СТОЛБЕЦ 5: Медленное (CheckBox, виден только если useStream = true)
            QCheckBox *slowPoolCheck = new QCheckBox();
            slowPoolCheck->setChecked(conn.slowPool);
            slowPoolCheck->setEnabled(conn.useStream);  // ✅ Активен только если Поток = true
            // ✅ Контейнер для CheckBox с фоном строки
            QWidget *slowPoolContainer = new QWidget();
            slowPoolContainer->setStyleSheet(
                QString("background-color: %1;").arg(rowColor.name())
            );
            QHBoxLayout *slowPoolLayout = new QHBoxLayout(slowPoolContainer);
            slowPoolLayout->setContentsMargins(0, 0, 0, 0);
            slowPoolLayout->addWidget(slowPoolCheck);
            slowPoolLayout->setAlignment(slowPoolCheck, Qt::AlignCenter);
            equipmentTable->setCellWidget(i, 5, slowPoolContainer);

            // ✅ Подключение сигнала: при изменении "Поток" — включать/выключать "Медленное"
            connect(streamCheck, &QCheckBox::toggled, this, [this, i, slowPoolCheck](bool checked) {
                slowPoolCheck->setEnabled(checked);
                if (!checked) {
                    slowPoolCheck->setChecked(false);  // ✅ Снимаем галку, если Поток выключен
                }
            });

            // ✅ СТОЛБЕЦ 6: Параметры (Button)
            QPushButton *paramsBtn = new QPushButton("Параметры...");
            paramsBtn->setStyleSheet(
                "QPushButton { background-color: #6A1B9A; color: white; "
                "padding: 4px 12px; border-radius: 3px; font-weight: bold; }"
                "QPushButton:hover { background-color: #4A148C; }"
            );
            connect(paramsBtn, &QPushButton::clicked, [this, i]() {
                openConnectionParamsDialog(i);
            });
            equipmentTable->setCellWidget(i, 6, paramsBtn);

            // ✅ КРИТИЧЕСКИ ВАЖНО: откладываем setVisible() до завершения отрисовки таблицы
            // Это гарантирует, что таблица уже отрисовалась и не принудительно покажет виджет
            QTimer::singleShot(0, this, [paramsBtn, conn]() {
                paramsBtn->setVisible(conn.useStream);
            });

            // ✅ Подключение сигнала: при изменении "Поток" — показывать/скрывать "Параметры"
            connect(streamCheck, &QCheckBox::toggled, this, [this, i, paramsBtn](bool checked) {
                paramsBtn->setVisible(checked);
            });
        }
        equipmentTable->resizeColumnsToContents();
    }

    // ✅ Открытие диалога параметров соединения
    void openConnectionParamsDialog(int rowIndex)
    {
        // Получаем текущие данные из таблицы
        EquipmentConnection conn;
        conn.name = equipmentTable->item(rowIndex, 0)->text();

        // ✅ Столбец 1: Подключено (CheckBox внутри контейнера)
        if (QWidget *container = equipmentTable->cellWidget(rowIndex, 1)) {
            if (QCheckBox *check = container->findChild<QCheckBox*>()) {
                conn.connected = check->isChecked();
            }
        }
        if (QLineEdit *edit = qobject_cast<QLineEdit*>(equipmentTable->cellWidget(rowIndex, 2))) {
            conn.address = edit->text();
            conn.ipAddress = edit->text(); // Синхронизируем с address
        }
        // ✅ Столбец 3: Оборудование (текст + deviceId)
        if (QComboBox *combo = qobject_cast<QComboBox*>(equipmentTable->cellWidget(rowIndex, 3))) {
            conn.equipmentType = combo->currentText();
            conn.deviceId = combo->currentData().toInt();  // ✅ Получаем код устройства
        }
        // ✅ Столбец 4: Поток (CheckBox внутри контейнера)
        if (QWidget *container = equipmentTable->cellWidget(rowIndex, 4)) {
            if (QCheckBox *check = container->findChild<QCheckBox*>()) {
                conn.useStream = check->isChecked();
            }
        }

        // ✅ Загружаем параметры соединения из m_connections
        if (rowIndex < m_connections.size()) {
            const EquipmentConnection &savedConn = m_connections[rowIndex];
            conn.interface = savedConn.interface;
            conn.ipAddress = savedConn.ipAddress;
            conn.port = savedConn.port;
            conn.snmpV1Enabled = savedConn.snmpV1Enabled;
            conn.snmpV1CommunityRead = savedConn.snmpV1CommunityRead;
            conn.snmpV1CommunityWrite = savedConn.snmpV1CommunityWrite;
            conn.snmpV3Enabled = savedConn.snmpV3Enabled;
            conn.snmpV3Username = savedConn.snmpV3Username;
            conn.snmpV3AuthProtocol = savedConn.snmpV3AuthProtocol;
            conn.snmpV3AuthPassword = savedConn.snmpV3AuthPassword;
            conn.snmpV3PrivProtocol = savedConn.snmpV3PrivProtocol;
            conn.snmpV3PrivPassword = savedConn.snmpV3PrivPassword;
        }

        // Открываем диалог
        ConnectionParamsDialog dlg(this);
        dlg.setConnectionData(conn);

        if (dlg.exec() == QDialog::Accepted) {
            // ✅ Получаем обновлённые данные
            EquipmentConnection updatedConn = dlg.getConnectionData();

            // Сохраняем в m_connections
            if (rowIndex < m_connections.size()) {
                m_connections[rowIndex].interface = updatedConn.interface;
                m_connections[rowIndex].ipAddress = updatedConn.ipAddress;
                m_connections[rowIndex].port = updatedConn.port;
                m_connections[rowIndex].snmpV1Enabled = updatedConn.snmpV1Enabled;
                m_connections[rowIndex].snmpV1CommunityRead = updatedConn.snmpV1CommunityRead;
                m_connections[rowIndex].snmpV1CommunityWrite = updatedConn.snmpV1CommunityWrite;
                m_connections[rowIndex].snmpV3Enabled = updatedConn.snmpV3Enabled;
                m_connections[rowIndex].snmpV3Username = updatedConn.snmpV3Username;
                m_connections[rowIndex].snmpV3AuthProtocol = updatedConn.snmpV3AuthProtocol;
                m_connections[rowIndex].snmpV3AuthPassword = updatedConn.snmpV3AuthPassword;
                m_connections[rowIndex].snmpV3PrivProtocol = updatedConn.snmpV3PrivProtocol;
                m_connections[rowIndex].snmpV3PrivPassword = updatedConn.snmpV3PrivPassword;

                // ✅ Синхронизируем IP-адрес с полем "Адрес" в таблице
                if (updatedConn.interface == "TCP/IP") {
                    if (QLineEdit *edit = qobject_cast<QLineEdit*>(
                            equipmentTable->cellWidget(rowIndex, 2))) {
                        edit->setText(updatedConn.ipAddress);
                    }
                }
            }

            qDebug() << "[EquipmentConfig] Параметры соединения обновлены для строки" << rowIndex;
        }
    }



private slots:
void onSave()
{
    // ✅ ПРОВЕРКА АВТОРИЗАЦИИ
    if (m_currentUser.isEmpty()) {
        QMessageBox::warning(this, "Требуется авторизация",
            "Для сохранения изменений необходимо авторизоваться.\n\n"
            "Выберите пользователя и введите пароль в главном окне.");
        return;
    }

    if (m_documentFilePath.isEmpty()) {
        QMessageBox::critical(this, "Ошибка", "Не указан файл документа.");
        return;
    }

    // ✅ Собираем данные из таблицы
    QList<EquipmentConnection> updatedConnections;
    for (int i = 0; i < equipmentTable->rowCount(); ++i) {
        EquipmentConnection conn;

        // Столбец 0: Название
        if (QTableWidgetItem *nameItem = equipmentTable->item(i, 0)) {
            conn.name = nameItem->text();
        }

        // ✅ Столбец 1: Подключено (CheckBox внутри контейнера QWidget)
        if (QWidget *container = equipmentTable->cellWidget(i, 1)) {
            if (QCheckBox *check = container->findChild<QCheckBox*>()) {
                conn.connected = check->isChecked();
            }
        }

        // Столбец 2: Адрес
        if (QLineEdit *edit = qobject_cast<QLineEdit*>(equipmentTable->cellWidget(i, 2))) {
            conn.address = edit->text();
        }

        // ✅ Столбец 3: Оборудование (текст + deviceId)
        if (QComboBox *combo = qobject_cast<QComboBox*>(equipmentTable->cellWidget(i, 3))) {
            conn.equipmentType = combo->currentText();
            conn.deviceId = combo->currentData().toInt();  // ✅ Получаем код устройства
        }

        // ✅ Столбец 4: Поток (CheckBox внутри контейнера QWidget)
        if (QWidget *container = equipmentTable->cellWidget(i, 4)) {
            if (QCheckBox *check = container->findChild<QCheckBox*>()) {
                conn.useStream = check->isChecked();
            }
        }

        // ✅ НОВЫЙ СТОЛБЕЦ 5: Медленное (CheckBox внутри контейнера QWidget)
        if (QWidget *container = equipmentTable->cellWidget(i, 5)) {
            if (QCheckBox *check = container->findChild<QCheckBox*>()) {
                conn.slowPool = check->isChecked();
            }
        }
        // ✅ НОВЫЕ ПОЛЯ: берём из m_connections
        if (i < m_connections.size()) {
            conn.interface = m_connections[i].interface;
            conn.ipAddress = m_connections[i].ipAddress;
            conn.port = m_connections[i].port;
            conn.snmpV1Enabled = m_connections[i].snmpV1Enabled;
            conn.snmpV1CommunityRead = m_connections[i].snmpV1CommunityRead;
            conn.snmpV1CommunityWrite = m_connections[i].snmpV1CommunityWrite;
            conn.snmpV3Enabled = m_connections[i].snmpV3Enabled;
            conn.snmpV3Username = m_connections[i].snmpV3Username;
            conn.snmpV3AuthProtocol = m_connections[i].snmpV3AuthProtocol;
            conn.snmpV3AuthPassword = m_connections[i].snmpV3AuthPassword;
            conn.snmpV3PrivProtocol = m_connections[i].snmpV3PrivProtocol;
            conn.snmpV3PrivPassword = m_connections[i].snmpV3PrivPassword;
        }

        // ✅ СБРАСЫВАЕМ КОД ОШИБКИ (если была)
        conn.errorCode = 0;

        updatedConnections.append(conn);
    }

    // ✅ Находим виджет документа
    MainWindow *mainWin = qobject_cast<MainWindow*>(parent());
    BaseDocWidget *doc = nullptr;
    if (mainWin) {
        for (QMdiSubWindow *sw : mainWin->findChildren<QMdiSubWindow*>()) {
            if (BaseDocWidget *w = qobject_cast<BaseDocWidget*>(sw->widget())) {
                if (w->filePath() == m_documentFilePath) {
                    doc = w;
                    break;
                }
            }
        }
    }

    if (!doc) {
        QMessageBox::critical(this, "Ошибка", "Не удалось найти виджет документа.");
        return;
    }

    // ✅ Обновляем массив подключений в виджете
    doc->setEquipmentConnections(updatedConnections);

    // ✅ Сохраняем в файл
    QFile file(m_documentFilePath);
    QJsonObject obj;
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument docJson = QJsonDocument::fromJson(file.readAll());
        obj = docJson.object();
        file.close();
    }

    // Преобразуем в JSON массив
    QJsonArray connectionsArray;
    for (int i = 0; i < updatedConnections.size(); ++i) {
        const EquipmentConnection &conn = updatedConnections[i];
        QJsonObject connObj;
        connObj["index"] = i; // ✅ Сохраняем правильный индекс
        connObj["name"] = conn.name;
        connObj["connected"] = conn.connected;
        connObj["address"] = conn.address;
        connObj["equipmentType"] = conn.equipmentType;
        connObj["deviceId"] = conn.deviceId;  // ✅ НОВОЕ: сохраняем код устройства
        connObj["useStream"] = conn.useStream;
        connObj["slowPool"] = conn.slowPool;  // ✅ НОВОЕ: сохраняем поле
        connObj["interface"] = conn.interface;
        connObj["ipAddress"] = conn.ipAddress;
        connObj["port"] = conn.port;
        connObj["snmpV1Enabled"] = conn.snmpV1Enabled;
        connObj["snmpV1CommunityRead"] = conn.snmpV1CommunityRead;
        connObj["snmpV1CommunityWrite"] = conn.snmpV1CommunityWrite;
        connObj["snmpV3Enabled"] = conn.snmpV3Enabled;
        connObj["snmpV3Username"] = conn.snmpV3Username;
        connObj["snmpV3AuthProtocol"] = conn.snmpV3AuthProtocol;
        connObj["snmpV3AuthPassword"] = conn.snmpV3AuthPassword;
        connObj["snmpV3PrivProtocol"] = conn.snmpV3PrivProtocol;
        connObj["snmpV3PrivPassword"] = conn.snmpV3PrivPassword;
        connectionsArray.append(connObj);
    }

    obj["connections"] = connectionsArray;

    // Записываем обратно
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson());
        file.close();

        // ✅ ПРОВЕРЯЕМ СОСТОЯНИЕ "В РАБОТЕ" И КРАСИМ СООТВЕТСТВУЮЩЕ
        if (doc->isInWork()) {
            doc->setEquipmentState(1); // ✅ Зелёный (оборудование в работе)
            qDebug() << "[EquipmentConfig] Заголовок: ЗЕЛЁНЫЙ (в работе)";
        } else {
            doc->setEquipmentState(0); // ✅ Синий (дежурный режим)
            qDebug() << "[EquipmentConfig] Заголовок: СИНИЙ (дежурный)";
        }

        QMessageBox::information(this, "Успех",
            QString("Конфигурация оборудования сохранена.\n"
                    "Авторизован как: %1")
                .arg(m_currentUser));
        accept();
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить конфигурацию.");
    }
}
};

#endif // EQUIPMENTCONFIGDIALOG_H
