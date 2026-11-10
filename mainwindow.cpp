#include "mainwindow.h"
#include "screenconfigdialog.h"
#include "antennawidget.h"
#include "peredatchikwidget.h"
#include "modulatorwidget.h"
#include "energowidget.h"
#include "authdialog.h"
#include "databasemanager.h"
#include "usermanagementdialog.h"
#include "changepassworddialog.h"
#include "equipmentsettingsdialog.h"
#include "equipmentconfigdialog.h"
#include "serverclientconfigdialog.h"
#include "databaseconfigdialog.h"
#include "databaseinitwizard.h"
#include "alarmregistry.h"
#include "dataviewdialog.h"
#include "useractionlogdialog.h"
#include "driverconfigeditordialog.h"
#include <QSqlDatabase>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QFileDialog>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <QPolygon>
#include <QGraphicsDropShadowEffect>

MainWindow::MainWindow(AppMode mode, const QJsonObject &clientConfig, QWidget *parent)
    : QMainWindow(parent),
      m_net(new NetworkController(mode, this)),
      m_mode(mode)
{
    // ✅ Загружаем конфиг первым делом — от него зависит состав вкладок.
    loadAppConfig();
    loadConfig();

    // ========================================================================
    // ✅ Создаём инфраструктуру групп: QTabBar сверху + QStackedWidget снизу.
    // ========================================================================
    QWidget *central = new QWidget(this);
    QVBoxLayout *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    m_groupTabs = new AlarmTabBar(central);
    m_groupTabs->setExpanding(false);
    m_groupTabs->setDrawBase(true);
    m_groupStack = new QStackedWidget(central);

    centralLayout->addWidget(m_groupTabs);
    centralLayout->addWidget(m_groupStack, 1);

    setCentralWidget(central);

    // ========================================================================
    // ✅ Создаём QMdiArea для каждой группы из config.groups.
    // ========================================================================
    m_groupAreas.clear();
    m_groupCriticalCount.clear();

    for (int i = 0; i < config.groups.size(); ++i) {
        const DocGroup &g = config.groups[i];

        QMdiArea *area = new QMdiArea(this);
        area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        area->setOption(QMdiArea::DontMaximizeSubWindowOnActivation, true);

        m_groupStack->addWidget(area);
        m_groupAreas.append(area);
        m_groupCriticalCount.append(0);

        m_groupTabs->addTab(g.name);

        // Пересчёт заголовка окна при активации подокна в любой группе.
        connect(area, &QMdiArea::subWindowActivated,
                this, &MainWindow::updateWindowTitle);
    }

    // скрываем вкладки, если группа ровно одна
    updateGroupTabsVisibility();

    // Переключение вкладок
    connect(m_groupTabs, &QTabBar::currentChanged,
            this, [this](int index) {
        if (index >= 0 && index < m_groupStack->count()) {
            m_groupStack->setCurrentIndex(index);
            updateMenus();
            updateWindowTitle();

            // Пересчитываем раскладку активной группы — на случай,
            // если размеры QMdiArea изменились.
            applyGridLayout(index);
        }
    });

    // ✅ Таймер мигания аварийных вкладок
    m_tabBlinkTimer = new QTimer(this);
    m_tabBlinkTimer->setInterval(500);
    connect(m_tabBlinkTimer, &QTimer::timeout,
            this, &MainWindow::onTabBlinkTimeout);
    m_tabBlinkTimer->start();

    // ✅ Если групп нет (битый конфиг) — создаём дефолтную.
    if (config.groups.isEmpty()) {
        DocGroup defaultGroup;
        defaultGroup.name = "Группа 1";
        defaultGroup.columns = 2;
        config.groups.append(defaultGroup);
    }

    updateWindowTitle();
    setWindowTitle(mode == AppMode::Server ? "rmo [SERVER]" : "rmo [CLIENT]");

    if (!QFileInfo(getAppDir()).isWritable()) {
        QMessageBox::warning(this, "Внимание", "Папка приложения недоступна для записи.");
    }

    setupActions();
    loadDatabaseConfig();

    bool dbConnected = DatabaseManager::instance().connect(
        m_dbConfig.host,
        m_dbConfig.port,
        m_dbConfig.dbName,
        m_dbConfig.user,
        m_dbConfig.password
    );

    if (dbConnected) {
        QStringList users = DatabaseManager::instance().getUsernames();
        authComboBox->blockSignals(true);
        authComboBox->clear();
        authComboBox->addItems(users);
        authComboBox->setCurrentIndex(-1);
        authComboBox->blockSignals(false);
        // Смену принял — тот же список.
        if (shiftComboBox) {
            shiftComboBox->blockSignals(true);
            shiftComboBox->clear();
            shiftComboBox->addItems(users);
            shiftComboBox->setCurrentIndex(-1);
            shiftComboBox->blockSignals(false);
        }
        qDebug() << "[Auth] Загружено пользователей из БД:" << users.size();
    } else {
        qDebug() << "[DB] База данных недоступна, запускаем мастер инициализации...";
        DatabaseInitWizard wizard(m_dbConfig.host, m_dbConfig.port, m_dbConfig.dbName, this);
        int wizardResult = wizard.exec();

        if (wizardResult == QDialog::Accepted && wizard.isInitialized()) {
            qDebug() << "[DB] Повторная попытка подключения после инициализации...";
            loadDatabaseConfig();
            dbConnected = DatabaseManager::instance().connect(
                m_dbConfig.host,
                m_dbConfig.port,
                m_dbConfig.dbName,
                m_dbConfig.user,
                m_dbConfig.password
            );

            if (dbConnected) {
                QStringList users = DatabaseManager::instance().getUsernames();
                authComboBox->blockSignals(true);
                authComboBox->clear();
                authComboBox->addItems(users);
                authComboBox->setCurrentIndex(-1);
                authComboBox->blockSignals(false);
                // Смену принял — тот же список.
                if (shiftComboBox) {
                    shiftComboBox->blockSignals(true);
                    shiftComboBox->clear();
                    shiftComboBox->addItems(users);
                    shiftComboBox->setCurrentIndex(-1);
                    shiftComboBox->blockSignals(false);
                }
                qDebug() << "[Auth] Загружено пользователей из БД:" << users.size();
            } else {
                authComboBox->blockSignals(true);
                authComboBox->clear();
                authComboBox->setCurrentIndex(-1);
                authComboBox->blockSignals(false);
                // Смену принял — тот же список.
                if (shiftComboBox) {
                    shiftComboBox->blockSignals(true);
                    shiftComboBox->clear();
                    shiftComboBox->setCurrentIndex(-1);
                    shiftComboBox->blockSignals(false);
                }
                QMessageBox::critical(this, "Критическая ошибка",
                    "База данных была создана, но не удалось подключиться.\n"
                    "Проверьте настройки в меню 'Конфигурация' → 'Конфигурация базы данных'.\n"
                    "Программа продолжит работу без авторизации.");
            }
        } else {
            authComboBox->blockSignals(true);
            authComboBox->clear();
            authComboBox->setCurrentIndex(-1);
            authComboBox->blockSignals(false);
            // Смену принял — тот же список.
            if (shiftComboBox) {
                shiftComboBox->blockSignals(true);
                shiftComboBox->clear();
                shiftComboBox->setCurrentIndex(-1);
                shiftComboBox->blockSignals(false);
            }
            QMessageBox::warning(this, "Ошибка БД",
                "Не удалось подключиться к базе данных PostgreSQL.\n"
                "Список пользователей не загружен.\n"
                "Вы можете настроить БД позже через меню:\n"
                "'Конфигурация' → 'Конфигурация базы данных'.");
        }
    }

    authTimeSeconds = 0;
    authTimer = new QTimer(this);
    connect(authTimer, &QTimer::timeout, this, &MainWindow::onAuthTimerTimeout);

    if (mode == AppMode::Server) {
        int serverPort = 5000;
        QString configPath = QCoreApplication::applicationDirPath() + "/rmo_config.json";
        QFile configFile(configPath);
        if (configFile.exists() && configFile.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
            serverPort = doc.object().value("serverPort").toInt(5000);
            configFile.close();
        }
        m_net->startServer(serverPort);
        qDebug() << "[Server] Запущен на порту" << serverPort;
        loadConfig();

        // Сначала — собрать «мёртвые» пути, потом удалить.
        QStringList missingPaths;
        for (const DocGroup &g : config.groups) {
            for (const QString &path : g.docPaths) {
                if (!QFileInfo::exists(path)) {
                    missingPaths.append(path);
                }
            }
        }
        for (const QString &p : missingPaths) {
            removeDocumentPath(p);
        }

        // Открываем существующие — каждый в своей группе.
        for (int gi = 0; gi < config.groups.size(); ++gi) {
            const QStringList paths = config.groups[gi].docPaths;  // копия!
            for (const QString &path : paths) {
                createDocumentFromFile(path, DocType::Antenna, gi, false);
            }
        }

        showMaximized();
        QTimer::singleShot(100, this, &MainWindow::applyGridLayoutAll);
    } else {
        QString ip = clientConfig.value("server_ip").toString("127.0.0.1");
        quint16 port = clientConfig.value("server_port").toInt(5000);
        m_net->connectToServer(ip, port);
        qDebug() << "[Client] Подключение к" << ip << ":" << port;
        connect(m_net, &NetworkController::configReceived, this, [this](const QByteArray &data){
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(data, &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                config.columns = obj.value("columns").toInt(2);
                QJsonArray docs = obj.value("documents").toArray();
                for (const QJsonValue &v : docs) {
                    QJsonObject d = v.toObject();
                    DocType t = DocType::Antenna;
                    QString typeStr = d["type"].toString();
                    if (typeStr == "transmitter") t = DocType::Transmitter;
                    else if (typeStr == "modulator") t = DocType::Modulator;
                    else if (typeStr == "energo") t = DocType::Energo;

                    // ✅ Пока — все документы в первую группу.
                    openDocumentFromConfig(d["path"].toString(), t, d["port"].toInt(0));
                }
                applyGridLayoutAll();
                showMaximized();
            }
        });
    }

    applyUserPermissions();
}

MainWindow::~MainWindow()
{
    qDebug() << "[MainWindow] Деструктор НАЧАЛО";

    // ✅ КРИТИЧЕСКИ ВАЖНО: Отключаем все сигналы destroyed ПЕРЕД уничтожением mdiArea
    for (QMdiSubWindow *sw : activeArea()->subWindowList()) {
        disconnect(sw, &QMdiSubWindow::destroyed, nullptr, nullptr);
    }

    if (m_mode == AppMode::Server) {
        saveConfig();
    }

    qDebug() << "[MainWindow] Деструктор ЗАВЕРШЕНИЕ";
}

static QIcon createEquipmentIcon(const QString &type)
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    if (type == "antenna") {
        p.setBrush(QColor("#005A9E"));
        p.drawRect(11, 12, 2, 10);
        QPolygon poly;
        poly << QPoint(12, 2) << QPoint(4, 12) << QPoint(20, 12);
        p.drawPolygon(poly);
    }
    else if (type == "transmitter") {
        p.setBrush(QColor("#2E7D32"));
        p.drawRect(6, 10, 12, 12);
        p.setBrush(Qt::NoBrush);
        QPen pen(QColor("#2E7D32"), 1.5);
        p.setPen(pen);
        p.drawArc(8, 4, 8, 8, -60 * 16, 120 * 16);
        p.drawArc(10, 2, 4, 4, -60 * 16, 120 * 16);
    }
    else if (type == "modulator") {
        p.setBrush(QColor("#6A1B9A"));
        p.drawRect(4, 6, 16, 12);
        p.setBrush(Qt::NoBrush);
        QPen pen(Qt::white, 1.5);
        p.setPen(pen);
        QPainterPath path;
        path.moveTo(6, 12);
        path.cubicTo(8, 6, 10, 18, 12, 12);
        path.cubicTo(14, 6, 16, 18, 18, 12);
        p.drawPath(path);
    }
    else if (type == "energo") {
        p.setBrush(QColor("#E65100"));
        QPolygon poly;
        poly << QPoint(13, 2) << QPoint(7, 12) << QPoint(11, 12)
             << QPoint(9, 22) << QPoint(17, 10) << QPoint(13, 10);
        p.drawPolygon(poly);
    }
    else if (type == "open") {
        p.setBrush(QColor("#546E7A"));
        QPainterPath path;
        path.moveTo(2, 6);
        path.lineTo(2, 20);
        path.lineTo(22, 20);
        path.lineTo(22, 8);
        path.lineTo(12, 8);
        path.lineTo(10, 6);
        path.lineTo(2, 6);
        path.closeSubpath();
        p.drawPath(path);
    }
    else if (type == "save") {
        p.setBrush(QColor("#546E7A"));
        p.drawRect(4, 4, 16, 18);
        p.setBrush(Qt::white);
        p.drawRect(7, 4, 10, 6);
        p.setBrush(QColor("#546E7A"));
        p.drawRect(9, 14, 6, 6);
    }
    else if (type == "config") {
        p.setBrush(QColor("#546E7A"));
        p.drawEllipse(6, 6, 12, 12);
        p.setBrush(Qt::white);
        p.drawEllipse(9, 9, 6, 6);
    }
    else if (type == "logout") {
        p.setBrush(QColor("#C62828"));
        p.drawRect(5, 6, 10, 16);
        p.setBrush(Qt::white);
        p.drawEllipse(12, 14, 2, 2);
        p.setBrush(QColor("#C62828"));
        QPolygon arrow;
        arrow << QPoint(17, 10) << QPoint(21, 13) << QPoint(17, 16);
        p.drawPolygon(arrow);
        p.fillRect(15, 12, 6, 2, QColor("#C62828"));
    }
    else if (type == "graph") {
        // ✅ Иконка "график" — стилизованная осциллограмма
        p.setBrush(QColor("#546E7A"));
        p.setPen(Qt::NoPen);

        // Рамка графика
        p.drawRect(2, 4, 20, 16);

        // Внутренняя область
        p.setBrush(Qt::white);
        p.drawRect(4, 6, 16, 12);

        // Оси
        QPen axisPen(QColor("#546E7A"), 1);
        p.setPen(axisPen);
        p.drawLine(4, 17, 20, 17);  // ось X
        p.drawLine(4, 6, 4, 17);    // ось Y

        // Кривая графика
        QPen linePen(QColor("#005A9E"), 2);
        p.setPen(linePen);
        QPainterPath curve;
        curve.moveTo(5, 15);
        curve.cubicTo(8, 8, 12, 16, 15, 10);
        curve.cubicTo(17, 7, 18, 12, 19, 8);
        p.drawPath(curve);
    }

    p.end();
    return QIcon(pixmap);
}

QString MainWindow::getAppDir() const { return QCoreApplication::applicationDirPath(); }
QString MainWindow::getConfigPath() const { return getAppDir() + "/config_monitor"; }

void MainWindow::addDocumentPath(const QString &path, int groupId)
{
    QString clean = QDir::cleanPath(QDir::toNativeSeparators(path));
    if (clean.isEmpty()) return;

    if (groupId < 0 || groupId >= config.groups.size()) {
        groupId = 0;   // дефолт: первая группа
    }

    DocGroup &g = config.groups[groupId];
    if (!g.docPaths.contains(clean)) {
        g.docPaths.append(clean);
        saveConfig();
    }
}

void MainWindow::removeDocumentPath(const QString &path)
{
    QString clean = QDir::cleanPath(QDir::toNativeSeparators(path));
    if (clean.isEmpty()) return;

    bool changed = false;
    for (DocGroup &g : config.groups) {
        if (g.docPaths.removeAll(clean) > 0) {
            changed = true;
        }
    }
    if (changed) {
        saveConfig();
    }
}

BaseDocWidget* MainWindow::findWidgetByPath(const QString &path) const
{
    for (QMdiArea *area : m_groupAreas) {
        if (!area) continue;
        for (QMdiSubWindow *sw : area->subWindowList()) {
            auto *w = qobject_cast<BaseDocWidget*>(sw->widget());
            if (w && w->filePath() == path) return w;
        }
    }
    return nullptr;
}

QString MainWindow::getActiveDocumentPath()
{
    QMdiArea *area = activeArea();
    if (!area) return QString();

    QMdiSubWindow *activeSubWindow = area->activeSubWindow();
    if (!activeSubWindow) return QString();

    QWidget *widget = activeSubWindow->widget();
    if (!widget) return QString();

    if (BaseDocWidget *docWidget = qobject_cast<BaseDocWidget*>(widget)) {
        return docWidget->filePath();
    }
    return QString();
}

BaseDocWidget* MainWindow::getCurrentDocument() const
{
    QMdiArea *area = activeArea();
    if (!area) return nullptr;

    QMdiSubWindow *activeSubWindow = area->activeSubWindow();
    if (!activeSubWindow) return nullptr;

    QWidget *widget = activeSubWindow->widget();
    if (!widget) return nullptr;

    return qobject_cast<BaseDocWidget*>(widget);
}

void MainWindow::loadConfig()
{
    QFile f(getConfigPath());
    if (!f.open(QIODevice::ReadOnly)) {
        // Файла нет — создаём дефолтную конфигурацию: одна группа.
        config.groups.clear();
        DocGroup defaultGroup;
        defaultGroup.name = "Группа 1";
        defaultGroup.columns = 2;
        config.groups.append(defaultGroup);
        config.columns = 2;
        return;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[Config] Ошибка парсинга config_monitor:" << err.errorString();
        config.groups.clear();
        DocGroup defaultGroup;
        defaultGroup.name = "Группа 1";
        defaultGroup.columns = 2;
        config.groups.append(defaultGroup);
        config.columns = 2;
        return;
    }

    QJsonObject obj = doc.object();

    // ----------------------------------------------------------------
    // ✅ Новый формат: { "columns": N, "groups": [ {...}, {...} ] }
    // ----------------------------------------------------------------
    if (obj.contains("groups")) {
        config.columns = obj.value("columns").toInt(2);
        config.groups.clear();

        QJsonArray groupsArr = obj.value("groups").toArray();
        for (const QJsonValue &gVal : groupsArr) {
            QJsonObject gObj = gVal.toObject();

            DocGroup g;
            g.name    = gObj.value("name").toString("Группа");
            g.columns = gObj.value("columns").toInt(config.columns);

            QJsonArray docs = gObj.value("documents").toArray();
            for (const QJsonValue &dVal : docs) {
                QString path = QDir::cleanPath(
                    QDir::toNativeSeparators(dVal.toString()));
                if (!path.isEmpty()) {
                    g.docPaths.append(path);
                }
            }
            config.groups.append(g);
        }

        // Защита: если групп 0 (битый файл) — создаём одну дефолтную.
        if (config.groups.isEmpty()) {
            DocGroup defaultGroup;
            defaultGroup.name = "Группа 1";
            defaultGroup.columns = 2;
            config.groups.append(defaultGroup);
        }

        qDebug() << "[Config] Загружено групп:" << config.groups.size();
        return;
    }

    // ----------------------------------------------------------------
    // ✅ Старый формат: { "columns": N, "documents": [...] }
    //    Миграция: заворачиваем в одну группу "По умолчанию".
    // ----------------------------------------------------------------
    if (obj.contains("documents")) {
        config.columns = obj.value("columns").toInt(2);
        config.groups.clear();

        DocGroup g;
        g.name = "По умолчанию";
        g.columns = config.columns;

        QJsonArray arr = obj.value("documents").toArray();
        for (const QJsonValue &v : arr) {
            QString path = QDir::cleanPath(
                QDir::toNativeSeparators(v.toString()));
            if (!path.isEmpty()) {
                g.docPaths.append(path);
            }
        }
        config.groups.append(g);

        qDebug() << "[Config] Миграция из старого формата: одна группа";
        return;
    }

    // ----------------------------------------------------------------
    // Совсем пустой/неизвестный конфиг — дефолт.
    // ----------------------------------------------------------------
    config.columns = 2;
    config.groups.clear();
    DocGroup defaultGroup;
    defaultGroup.name = "Группа 1";
    defaultGroup.columns = 2;
    config.groups.append(defaultGroup);
}

void MainWindow::broadcastConfigToClients()
{
    // ⚠️ ВРЕМЕННАЯ РЕАЛИЗАЦИЯ: старый формат (плоский список).
    //    Сетевая синхронизация групп — отдельный этап.
    QJsonObject msg;
    msg["type"] = "config";
    msg["columns"] = config.columns;

    QJsonArray docs;
    for (const DocGroup &g : config.groups) {
        for (const QString &path : g.docPaths) {
            QJsonObject d;
            d["path"] = path;
            d["port"] = 0;

            BaseDocWidget *w = findWidgetByPath(path);
            if (w) {
                if (w->docType() == DocType::Antenna) d["type"] = "antenna";
                else if (w->docType() == DocType::Transmitter) d["type"] = "transmitter";
                else if (w->docType() == DocType::Modulator) d["type"] = "modulator";
                else if (w->docType() == DocType::Energo) d["type"] = "energo";
                d["port"] = w->getDocPort();
            }
            docs.append(d);
        }
    }
    msg["documents"] = docs;

    m_net->broadcastConfig(NetworkController::serializePacket(msg));
}

void MainWindow::saveConfig()
{
    QJsonObject obj;
    obj["columns"] = config.columns;

    // ----------------------------------------------------------------
    // ✅ Новый формат: groups[]
    // ----------------------------------------------------------------
    QJsonArray groupsArr;
    for (const DocGroup &g : config.groups) {
        QJsonObject gObj;
        gObj["name"] = g.name;
        gObj["columns"] = g.columns;

        QJsonArray docsArr;
        for (const QString &p : g.docPaths) {
            docsArr.append(QDir::cleanPath(QDir::toNativeSeparators(p)));
        }
        gObj["documents"] = docsArr;

        groupsArr.append(gObj);
    }
    obj["groups"] = groupsArr;

    QFile f(getConfigPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        f.close();
    }

    if (m_mode == AppMode::Server) {
        broadcastConfigToClients();
    }
}

void MainWindow::loadAppConfig()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/rmo_config.json";
    QFile file(configPath);

    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject obj = doc.object();
        m_programName = obj.value("programName").toString("RMO");
        file.close();
    } else {
        m_programName = "RMO";
    }

    qDebug() << "[Config] Название программы:" << m_programName;
}

void MainWindow::updateWindowTitle()
{
    QString modeStr = (m_mode == AppMode::Server) ? "SERVER" : "CLIENT";
    QString docName;

    BaseDocWidget *doc = getCurrentDocument();
    if (doc) {
        QString filePath = doc->filePath();
        if (!filePath.isEmpty()) {
            docName = QFileInfo(filePath).completeBaseName();
        }
    }

    QString title = m_programName + " [" + modeStr + "]";
    if (!docName.isEmpty()) {
        title += " - " + docName;
    }
    setWindowTitle(title);
}

BaseDocWidget* MainWindow::createDocumentFromFile(const QString &path,
                                                  DocType type,
                                                  int groupId,
                                                  bool addToConfig)
{
    // Определяем тип по расширению, если путь задан.
    if (!path.isEmpty()) {
        QString ext = QFileInfo(path).suffix().toLower();
        if (ext == "ant") type = DocType::Antenna;
        else if (ext == "per") type = DocType::Transmitter;
        else if (ext == "mod") type = DocType::Modulator;
        else if (ext == "enr") type = DocType::Energo;
    }

    // Группа по умолчанию — активная.
    if (groupId < 0) {
        groupId = m_groupTabs ? m_groupTabs->currentIndex() : 0;
    }
    if (groupId < 0 || groupId >= m_groupAreas.size()) {
        groupId = 0;
    }
    QMdiArea *area = m_groupAreas[groupId];
    if (!area) return nullptr;

    BaseDocWidget *doc = nullptr;
    switch (type) {
        case DocType::Antenna:     doc = new AntennaWidget(this); break;
        case DocType::Transmitter: doc = new PeredatchikWidget(this); break;
        case DocType::Modulator:   doc = new ModulatorWidget(this); break;
        case DocType::Energo:      doc = new EnergoWidget(this); break;
    }
    if (!doc) return nullptr;

    QString finalTitle = path.isEmpty() ? "Новый документ"
                                        : QFileInfo(path).completeBaseName();
    CustomTitleBar *titleBar = new CustomTitleBar(finalTitle, doc);
    doc->setTitleBarWidget(titleBar);

    QSize targetSize = doc->documentFixedSize();
    titleBar->setMinimumWidth(targetSize.width() - 12);

    // ✅ Подключаем кнопку звука в заголовке к проигрывателю аварий документа.
    connect(titleBar, &CustomTitleBar::soundStateChanged,
            doc, &BaseDocWidget::setAlarmSoundEnabled);

    if (!path.isEmpty()) {
        doc->setFilePath(path);
        initializeDocumentFile(path, type);
        doc->loadFromFilePreserveSections(path);
        if (addToConfig) addDocumentPath(path, groupId);
    } else if (addToConfig) {
        QString prefix, suffix;
        switch (type) {
            case DocType::Antenna:     prefix = "New_Ant"; suffix = ".ant"; break;
            case DocType::Transmitter: prefix = "New_Tx";  suffix = ".per"; break;
            case DocType::Modulator:   prefix = "New_Mod"; suffix = ".mod"; break;
            case DocType::Energo:      prefix = "New_Enr"; suffix = ".enr"; break;
        }
        QString fullPath = getAppDir() + QString("/%1_%2%3")
            .arg(prefix)
            .arg(QDateTime::currentMSecsSinceEpoch())
            .arg(suffix);
        doc->setFilePath(fullPath);
        initializeDocumentFile(fullPath, type);
        doc->loadFromFilePreserveSections(fullPath);
        addDocumentPath(fullPath, groupId);
    }

    FixedSubWindow *sub = new FixedSubWindow(doc, area);
    sub->setGroupId(groupId);
    sub->setAttribute(Qt::WA_DeleteOnClose);
    sub->setFixedSize(targetSize);

    if (QLayout *subLayout = sub->layout()) {
        subLayout->setContentsMargins(0, 0, 0, 0);
        subLayout->setAlignment(doc, Qt::AlignTop | Qt::AlignLeft);
    }

    if (QBoxLayout *boxLayout = qobject_cast<QBoxLayout*>(doc->layout())) {
        boxLayout->setContentsMargins(6, 6, 6, 12);
        boxLayout->insertWidget(0, titleBar);
    } else if (QLayout *docLayout = doc->layout()) {
        docLayout->setContentsMargins(6, 6, 6, 12);
        docLayout->addWidget(titleBar);
    }

    doc->setTitleIndicator(titleBar->getIndicator());
    sub->show();

    QString serverIp = "127.0.0.1";
    quint16 docPort = 6000 + (sub ? area->subWindowList().size() : 0);
    doc->initDocumentNetwork(m_mode, docPort, serverIp, 0);

    connect(doc, &BaseDocWidget::alarmsUpdated,
            this, &MainWindow::updateAlarmPanelForDoc);

    // Безопасный захват docPath при удалении подокна.
    QString docPath = QDir::cleanPath(QDir::toNativeSeparators(doc->filePath()));
    QPointer<MainWindow> safeThis(this);
    connect(sub, &QMdiSubWindow::destroyed, this, [safeThis, docPath]() {
        if (!safeThis) return;
        QTimer::singleShot(0, safeThis.data(), [safeThis, docPath]() {
            if (!safeThis) return;
            // ✅ Убираем документ из счётчиков аварий.
            safeThis->m_docCriticalCount.remove(docPath);
            const int groupId = safeThis->findGroupIdByDocPath(docPath);
            if (groupId >= 0) {
                safeThis->recalcGroupCriticalCount(groupId);
                safeThis->updateTabAppearance(groupId);
            }
            safeThis->removeDocumentPath(docPath);
            safeThis->applyGridLayoutAll();
            if (safeThis->m_mode == AppMode::Server) {
                safeThis->broadcastConfigToClients();
            }
        });
    });

    updateMenus();
    if (m_mode == AppMode::Server) {
        broadcastConfigToClients();
    }

    // Раскладка группы, в которую добавлен документ.
    QTimer::singleShot(20, this, [this, groupId]() { applyGridLayout(groupId); });

    return doc;
}

void MainWindow::initializeDocumentFile(const QString &filePath, DocType type)
{
    QFile file(filePath);

    if (file.exists()) {
        QJsonObject obj;
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            obj = doc.object();
            file.close();
        } else {
            qWarning() << "[DocInit] Не удалось открыть файл:" << filePath;
            return;
        }

        bool needSave = false;

        if (!obj.contains("settings")) {
            QJsonObject settings;
            settings["polling_interval"] = 1000;
            settings["db_write_interval"] = 5000;
            settings["alarm_write_duration_min"] = 10;
            settings["in_work"] = false;
            obj["settings"] = settings;
            needSave = true;
            qDebug() << "[DocInit] Добавлена секция settings в:" << filePath;
        } else {
            // ✅ Добавляем alarm_write_duration_min, если его нет
            //    (для документов, созданных до этого патча).
            QJsonObject settings = obj.value("settings").toObject();
            if (!settings.contains("alarm_write_duration_min")) {
                settings["alarm_write_duration_min"] = 10;
                obj["settings"] = settings;
                needSave = true;
                qDebug() << "[DocInit] Добавлено поле alarm_write_duration_min в:"
                         << filePath;
            }
        }

        if (!obj.contains("connections")) {
            QJsonArray connectionsArray;
            QStringList connectionNames = getEquipmentConnectionNames(type);
            int rowCount = connectionNames.size();

            for (int i = 0; i < rowCount; ++i) {
                QJsonObject connObj;
                connObj["index"] = i;
                connObj["name"] = connectionNames[i];
                connObj["connected"] = false;
                connObj["address"] = "";
                connObj["equipmentType"] = "";
                connObj["deviceId"] = ID_UNKNOWN;
                connObj["useStream"] = false;
                connObj["interface"] = "TCP/IP";
                connObj["ipAddress"] = "";
                connObj["port"] = 502;
                connObj["snmpV1Enabled"] = false;
                connObj["snmpV1CommunityRead"] = "public";
                connObj["snmpV1CommunityWrite"] = "private";
                connObj["snmpV3Enabled"] = false;
                connObj["snmpV3Username"] = "";
                connObj["snmpV3AuthProtocol"] = "MD5";
                connObj["snmpV3AuthPassword"] = "";
                connObj["snmpV3PrivProtocol"] = "DES";
                connObj["snmpV3PrivPassword"] = "";
                connObj["comPort"] = "";
                connObj["baudRate"] = 9600;
                connObj["dataBits"] = 8;
                connObj["parity"] = "None";
                connObj["stopBits"] = 1;
                connectionsArray.append(connObj);
            }

            obj["connections"] = connectionsArray;
            needSave = true;
            qDebug() << "[DocInit] Добавлена секция connections в:" << filePath;
        }

        if (needSave) {
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(obj).toJson());
                file.close();
                qDebug() << "[DocInit] Файл обновлён:" << filePath;
            } else {
                qWarning() << "[DocInit] Не удалось сохранить файл:" << filePath;
            }
        }
        return;
    }

    QJsonObject obj;
    obj["type"] = docTypeToString(type);

    QJsonObject settings;
    settings["polling_interval"] = 1000;
    settings["db_write_interval"] = 5000;
    settings["alarm_write_duration_min"] = 10;
    settings["in_work"] = false;
    obj["settings"] = settings;

    QJsonArray connectionsArray;
    QStringList connectionNames = getEquipmentConnectionNames(type);
    int rowCount = connectionNames.size();

    for (int i = 0; i < rowCount; ++i) {
        QJsonObject connObj;
        connObj["index"] = i;
        connObj["name"] = connectionNames[i];
        connObj["connected"] = false;
        connObj["address"] = "";
        connObj["equipmentType"] = "";
        connObj["deviceId"] = ID_UNKNOWN;
        connObj["useStream"] = false;
        connObj["interface"] = "TCP/IP";
        connObj["ipAddress"] = "";
        connObj["port"] = 502;
        connObj["snmpV1Enabled"] = false;
        connObj["snmpV1CommunityRead"] = "public";
        connObj["snmpV1CommunityWrite"] = "private";
        connObj["snmpV3Enabled"] = false;
        connObj["snmpV3Username"] = "";
        connObj["snmpV3AuthProtocol"] = "MD5";
        connObj["snmpV3AuthPassword"] = "";
        connObj["snmpV3PrivProtocol"] = "DES";
        connObj["snmpV3PrivPassword"] = "";
        connObj["comPort"] = "";
        connObj["baudRate"] = 9600;
        connObj["dataBits"] = 8;
        connObj["parity"] = "None";
        connObj["stopBits"] = 1;
        connectionsArray.append(connObj);
    }

    obj["connections"] = connectionsArray;
    obj["parameters"] = QJsonObject();

    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson());
        file.close();
        qDebug() << "[DocInit] Создан новый файл документа:" << filePath;
    } else {
        qWarning() << "[DocInit] Ошибка создания файла:" << filePath;
    }
}

QString MainWindow::docTypeToString(DocType type)
{
    switch (type) {
        case DocType::Antenna:     return "antenna";
        case DocType::Transmitter: return "transmitter";
        case DocType::Modulator:   return "modulator";
        case DocType::Energo:      return "energo";
        default:                   return "unknown";
    }
}

void MainWindow::openDocumentFromConfig(const QString &path, DocType type, quint16 docPort)
{
    BaseDocWidget *doc = nullptr;
    switch (type) {
        case DocType::Antenna:     doc = new AntennaWidget(this); break;
        case DocType::Transmitter: doc = new PeredatchikWidget(this); break;
        case DocType::Modulator:   doc = new ModulatorWidget(this); break;
        case DocType::Energo:      doc = new EnergoWidget(this); break;
    }

    if (!doc) return;

    doc->loadFromFile(path);
    doc->setFilePath(path);
    doc->initDocumentNetwork(AppMode::Client, 0, "127.0.0.1", docPort);

    FixedSubWindow *sub = new FixedSubWindow(doc, activeArea());
    sub->setAttribute(Qt::WA_DeleteOnClose);
    sub->setFixedSize(doc->documentFixedSize());
    sub->show();

    doc->setWindowTitle(QFileInfo(path).fileName());
    updateMenus();
}

void MainWindow::setupActions()
{
    mainToolBar = addToolBar("Main Toolbar");
    mainToolBar->setObjectName("MainToolBar");
    mainToolBar->setIconSize(QSize(24, 24));
    mainToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    mainToolBar->setMovable(false);
    mainToolBar->setStyleSheet(
        "QToolBar { "
        "  background-color: #F0F0F0; "
        "  border-bottom: 1px solid #D0D0D0; "
        "  spacing: 4px; "
        "  padding: 4px; "
        "} "
        "QToolButton { "
        "  border: 1px solid transparent; "
        "  border-radius: 4px; "
        "  padding: 4px; "
        "} "
        "QToolButton:hover { "
        "  background-color: #E0E0E0; "
        "  border: 1px solid #C0C0C0; "
        "} "
        "QToolButton:pressed { "
        "  background-color: #D0D0D0; "
        "}"
    );

    newAntAct = new QAction("Новая Антенна", this);
    newAntAct->setIcon(createEquipmentIcon("antenna"));
    connect(newAntAct, &QAction::triggered, this, &MainWindow::newAntenna);

    newTxAct = new QAction("Новый Передатчик", this);
    newTxAct->setIcon(createEquipmentIcon("transmitter"));
    connect(newTxAct, &QAction::triggered, this, &MainWindow::newTransmitter);

    newModAct = new QAction("Новый Модулятор", this);
    newModAct->setIcon(createEquipmentIcon("modulator"));
    connect(newModAct, &QAction::triggered, this, &MainWindow::newModulator);

    newEnrAct = new QAction("Новый Энергоблок", this);
    newEnrAct->setIcon(createEquipmentIcon("energo"));
    connect(newEnrAct, &QAction::triggered, this, &MainWindow::newEnergo);

    openAct = new QAction("&Открыть...", this);
    openAct->setIcon(createEquipmentIcon("open"));
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openDocument);

    configAct = new QAction("Конфигурация экрана...", this);
    configAct->setIcon(createEquipmentIcon("config"));
    connect(configAct, &QAction::triggered, this, &MainWindow::showScreenConfig);

    equipmentSettingsAct = new QAction("Настройка параметров", this);
    equipmentSettingsAct->setIcon(createEquipmentIcon("config"));
    connect(equipmentSettingsAct, &QAction::triggered, this, &MainWindow::showEquipmentSettings);

    // ✅ НОВЫЙ Action: Просмотр графика
    viewDataAct = new QAction("Просмотр графика", this);
    viewDataAct->setIcon(createEquipmentIcon("graph"));  // см. ниже — нужна иконка
    viewDataAct->setToolTip("Открыть отчёт с графиком для активного документа");
    viewDataAct->setShortcut(QKeySequence("Ctrl+G"));
    connect(viewDataAct, &QAction::triggered, this, &MainWindow::onViewDataAction);

    saveAct = new QAction("&Сохранить", this);
    saveAct->setIcon(createEquipmentIcon("save"));
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveDocument);

    saveAsAct = new QAction("Сохранить &как...", this);
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveAsDocument);

    closeAct = new QAction("&Закрыть", this);
    closeAct->setShortcut(QKeySequence::Close);
    connect(closeAct, &QAction::triggered, this, &MainWindow::closeDocument);

    QAction *exitAct = new QAction("Вы&ход", this);
    exitAct->setMenuRole(QAction::QuitRole);
    connect(exitAct, &QAction::triggered, this, &QMainWindow::close);

    fileMenu = menuBar()->addMenu("&Файл");
    QMenu *newMenu = fileMenu->addMenu("Новый");
    newMenu->addAction(newAntAct);
    newMenu->addAction(newTxAct);
    newMenu->addAction(newModAct);
    newMenu->addAction(newEnrAct);
    fileMenu->addAction(openAct);
    fileMenu->addSeparator();
    fileMenu->addAction(saveAct);
    fileMenu->addAction(saveAsAct);
    fileMenu->addSeparator();
    fileMenu->addAction(closeAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    mainToolBar->addAction(newAntAct);
    mainToolBar->addAction(newTxAct);
    mainToolBar->addAction(newModAct);
    mainToolBar->addAction(newEnrAct);
    mainToolBar->addSeparator();
    mainToolBar->addAction(openAct);
    mainToolBar->addAction(saveAct);
    mainToolBar->addSeparator();
    mainToolBar->addAction(equipmentSettingsAct);
    mainToolBar->addSeparator();
    // ✅ НОВОЕ: Отчёт с графиком
    mainToolBar->addAction(viewDataAct);
    mainToolBar->addSeparator();

    // ============================================================================
    // Смену принял
    // ============================================================================

    QLabel *shiftLabel = new QLabel("Смену принял:", this);
    shiftLabel->setStyleSheet(
        "font-weight: bold; color: #333333; margin-right: 4px; margin-left: 8px;");
    mainToolBar->addWidget(shiftLabel);

    shiftComboBox = new QComboBox(this);
    shiftComboBox->setMinimumWidth(150);
    shiftComboBox->setCurrentIndex(-1);
    shiftComboBox->setStyleSheet(
        "QComboBox { padding: 4px; border: 1px solid #B0B0B0; "
        "             border-radius: 3px; background-color: white; }"
        "QComboBox:hover { border: 1px solid #005A9E; }"
    );

    connect(shiftComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onShiftComboBoxChanged);

    mainToolBar->addWidget(shiftComboBox);
    mainToolBar->addSeparator();

    // ============================================================================
    // блок авторизации
    // ============================================================================
    authLabel = new QLabel("Авторизация:", this);
    authLabel->setStyleSheet("font-weight: bold; color: #333333; margin-right: 4px; margin-left: 8px;");
    mainToolBar->addWidget(authLabel);

    authComboBox = new QComboBox(this);
    authComboBox->setMinimumWidth(150);
    authComboBox->setCurrentIndex(-1);
    authComboBox->setStyleSheet(
        "QComboBox { "
        "  padding: 4px; "
        "  border: 1px solid #B0B0B0; "
        "  border-radius: 3px; "
        "  background-color: white; "
        "} "
        "QComboBox:hover { border: 1px solid #005A9E; }"
    );

    connect(authComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index < 0) {
            // ----------------------------------------------------------------
            // Авторизация снята (logout или таймаут).
            // ----------------------------------------------------------------
            if (!m_authUser.isEmpty()) {
                // Логируем «выход» под именем уходившего.
                DatabaseManager::instance().setCurrentUserName(m_authUser);
                DatabaseManager::instance().logUserAction(
                    "Пользователи", QString(),
                    QString("Выход из системы: %1").arg(m_authUser));
            }

            m_authUser.clear();

            // ✅ Имя и роль — теперь по смене (или Аноним).
            DatabaseManager::instance().setCurrentUserName(effectiveUserName());
            DatabaseManager::instance().setCurrentUserRole(effectiveUserRole());

            authTimer->stop();
            timeLabel->setText("00:00");
            timeLabel->setStyleSheet(
                "font-family: 'Consolas', 'Courier New', monospace; "
                "font-weight: bold; color: #999999; font-size: 15px; "
                "margin: 0px 8px;");

            applyUserPermissions();
            return;
        }

        // ----------------------------------------------------------------
        // Авторизация с паролем.
        // ----------------------------------------------------------------
        const QString username = authComboBox->currentText();
        AuthDialog authDlg(username, this);
        if (authDlg.exec() == QDialog::Accepted && authDlg.isAuthenticated()) {
            m_authUser = username;

            const QString role = DatabaseManager::instance().getUserRole(username);
            DatabaseManager::instance().setCurrentUserRole(role);
            DatabaseManager::instance().setCurrentUserName(username);

            DatabaseManager::instance().logUserAction(
                "Пользователи", QString(),
                QString("Вход в систему: %1").arg(username));

            authTimeSeconds = 1800;
            timeLabel->setText("30:00");
            timeLabel->setStyleSheet(
                "font-family: 'Consolas', 'Courier New', monospace; "
                "font-weight: bold; color: #005A9E; font-size: 15px; "
                "margin: 0px 8px;");
            authTimer->start(1000);

            applyUserPermissions();
        } else {
            authComboBox->blockSignals(true);
            authComboBox->setCurrentIndex(-1);
            authComboBox->blockSignals(false);

            // Возвращаемся к правам смены (если была) или к пустому состоянию.
            DatabaseManager::instance().setCurrentUserName(effectiveUserName());
            DatabaseManager::instance().setCurrentUserRole(effectiveUserRole());

            applyUserPermissions();
        }
    });

    mainToolBar->addWidget(authComboBox);
    mainToolBar->addSeparator();

    timeLabel = new QLabel("00:00", this);
    timeLabel->setStyleSheet(
        "font-family: 'Consolas', 'Courier New', monospace; "
        "font-weight: bold; "
        "color: #999999; "
        "font-size: 15px; "
        "margin: 0px 8px;"
    );
    mainToolBar->addWidget(timeLabel);

    logoutButton = new QToolButton(this);
    logoutButton->setIcon(createEquipmentIcon("logout"));
    logoutButton->setIconSize(QSize(24, 24));
    logoutButton->setToolTip("Выход из авторизации");
    logoutButton->setStyleSheet(
        "QToolButton { "
        "  background-color: transparent; "
        "  border: 1px solid transparent; "
        "  border-radius: 4px; "
        "  padding: 4px; "
        "} "
        "QToolButton:hover { "
        "  background-color: #FFEBEE; "
        "  border: 1px solid #C62828; "
        "} "
        "QToolButton:pressed { "
        "  background-color: #FFCDD2; "
        "}"
    );
    mainToolBar->addWidget(logoutButton);

    connect(logoutButton, &QToolButton::clicked, this, &MainWindow::onLogoutClicked);

    QMenu *securityMenu = menuBar()->addMenu("&Безопасность");

    changePasswordAct = new QAction("Сменить пароль...", this);
    connect(changePasswordAct, &QAction::triggered, this, &MainWindow::showChangePassword);
    securityMenu->addAction(changePasswordAct);
    securityMenu->addSeparator();

    userConfigAct = new QAction("Конфигурация доступа...", this);
    userConfigAct->setIcon(createEquipmentIcon("config"));
    connect(userConfigAct, &QAction::triggered, this, &MainWindow::showUserManagement);
    securityMenu->addAction(userConfigAct);

    securityMenu->addSeparator();

    // ✅ Журнал действий пользователей.
    QAction *userActionLogAct = new QAction("Журнал действий пользователей...", this);
    userActionLogAct->setIcon(createEquipmentIcon("graph"));  // опционально
    connect(userActionLogAct, &QAction::triggered,
            this, &MainWindow::showUserActionLog);
    securityMenu->addAction(userActionLogAct);

    QMenu *settingsMenu = menuBar()->addMenu("&Настройка");
    settingsMenu->addAction(equipmentSettingsAct);

    // ✅ НОВОЕ МЕНЮ: Отчёт
    QMenu *reportMenu = menuBar()->addMenu("&Отчёт");
    reportMenu->addAction(viewDataAct);

    // ✅ Меню «Сервис»: редактор конфигов драйверов
    QMenu *serviceMenu = menuBar()->addMenu("&Сервис");
    QAction *driverConfigAct = new QAction("Конфиги драйверов...", this);
    driverConfigAct->setIcon(createEquipmentIcon("config"));
    connect(driverConfigAct, &QAction::triggered,
            this, &MainWindow::showDriverConfigEditor);
    serviceMenu->addAction(driverConfigAct);

    QMenu *configMenu = menuBar()->addMenu("&Конфигурация");

    equipmentConfigAct = new QAction("Конфигурация оборудования...", this);
    connect(equipmentConfigAct, &QAction::triggered, this, &MainWindow::showEquipmentConfig);
    configMenu->addAction(equipmentConfigAct);

    configMenu->addAction(configAct);

    serverClientConfigAct = new QAction("Конфигурация сервер/клиент...", this);
    connect(serverClientConfigAct, &QAction::triggered, this, &MainWindow::showServerClientConfig);
    configMenu->addAction(serverClientConfigAct);

    databaseConfigAct = new QAction("Конфигурация базы данных...", this);
    connect(databaseConfigAct, &QAction::triggered, this, &MainWindow::showDatabaseConfig);
    configMenu->addAction(databaseConfigAct);

    windowMenu = menuBar()->addMenu("&Окно");
    windowMenu->addAction("&Каскадом", this, &MainWindow::cascadeWindows);
    windowMenu->addAction("&Мозаикой", this, &MainWindow::tileWindows);
    windowMenu->addSeparator();
    windowMenu->addAction("С&ледующее", this, &MainWindow::nextWindow);
    windowMenu->addAction("П&редыдущее", this, &MainWindow::previousWindow);
    windowMenu->addSeparator();
    connect(windowMenu, &QMenu::aboutToShow, this, &MainWindow::updateWindowMenu);

    menuBar()->addMenu("&Помощь")->addAction("О программе", this, &MainWindow::aboutApp);
}

void MainWindow::newAntenna()     { createDocumentFromFile(QString(), DocType::Antenna); QTimer::singleShot(20, this, &MainWindow::applyGridLayoutAll); }
void MainWindow::newTransmitter() { createDocumentFromFile(QString(), DocType::Transmitter); QTimer::singleShot(20, this, &MainWindow::applyGridLayoutAll); }
void MainWindow::newModulator()   { createDocumentFromFile(QString(), DocType::Modulator); QTimer::singleShot(20, this, &MainWindow::applyGridLayoutAll); }
void MainWindow::newEnergo()      { createDocumentFromFile(QString(), DocType::Energo); QTimer::singleShot(20, this, &MainWindow::applyGridLayoutAll); }

void MainWindow::openDocument()
{
    QString filter = "Все документы (*.ant *.per *.mod *.enr);;Антенна (*.ant);;Передатчик (*.per);;Модулятор (*.mod);;Энерго (*.enr)";
    QString filePath = QFileDialog::getOpenFileName(this, "Открыть документ", getAppDir(), filter);

    if (!filePath.isEmpty()) {
        int gid = m_groupTabs ? m_groupTabs->currentIndex() : 0;
        createDocumentFromFile(filePath, DocType::Antenna, gid);
        QTimer::singleShot(20, this, &MainWindow::applyGridLayoutAll);
    }
}

void MainWindow::saveDocument()
{
    QMdiSubWindow *activeSubWindow = activeArea() ? activeArea()->activeSubWindow() : nullptr;
    if (!activeSubWindow) return;

    BaseDocWidget *doc = qobject_cast<BaseDocWidget*>(activeSubWindow->widget());
    if (!doc) return;

    QString filePath = doc->filePath();
    if (filePath.isEmpty()) {
        saveAsDocument();
        return;
    }

    if (doc->saveToFilePreserveSections(filePath)) {
        qDebug() << "[Save] Документ сохранён:" << filePath;
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить документ.");
    }
}

void MainWindow::saveAsDocument()
{
    QMdiSubWindow *sub = activeArea() ? activeArea()->activeSubWindow() : nullptr;
    if (!sub) return;

    BaseDocWidget *doc = qobject_cast<BaseDocWidget*>(sub->widget());
    if (!doc) return;

    QString filter;
    switch (doc->docType()) {
        case DocType::Antenna:     filter = "Антенна (*.ant)"; break;
        case DocType::Transmitter: filter = "Передатчик (*.per)"; break;
        case DocType::Modulator:   filter = "Модулятор (*.mod)"; break;
        case DocType::Energo:      filter = "Энерго (*.enr)"; break;
    }

    QString filePath = QFileDialog::getSaveFileName(this, "Сохранить как",
        doc->filePath().isEmpty() ? getAppDir() : doc->filePath(), filter);

    if (!filePath.isEmpty()) {
        if (!filePath.contains(".")) {
            if (filter.contains("ant")) filePath += ".ant";
            else if (filter.contains("per")) filePath += ".per";
            else if (filter.contains("mod")) filePath += ".mod";
            else filePath += ".enr";
        }
        doc->setFilePath(filePath);
        doc->saveToFile();
    }
}

void MainWindow::closeDocument()
{
    QMdiSubWindow *sub = activeArea() ? activeArea()->activeSubWindow() : nullptr;
    if (sub) sub->close();
}

QStringList MainWindow::getDocPaths() const
{
    QStringList result;
    for (const DocGroup &g : config.groups) {
        result.append(g.docPaths);
    }
    return result;
}

void MainWindow::showScreenConfig()
{
    if (m_authUser.isEmpty()) {
        QMessageBox::warning(this, "Доступ запрещён",
            "Необходима авторизация для доступа к конфигурации экрана.");
        return;
    }

    if (!DatabaseManager::instance().hasPermission(m_authUser, "change_screen_config")) {
        QMessageBox::warning(this, "Доступ запрещён",
            "Только администратор может изменять конфигурацию экрана.");
        return;
    }

    ScreenConfigDialog dlg(config.groups, this);
    if (dlg.exec() == QDialog::Accepted) {
        applyScreenConfig(dlg.groups());
    }
}

void MainWindow::updateMenus()
{
    bool hasDocs = false;
    for (const DocGroup &g : config.groups) {
        if (!g.docPaths.isEmpty()) { hasDocs = true; break; }
    }
    saveAct->setEnabled(hasDocs);
    saveAsAct->setEnabled(hasDocs);
    closeAct->setEnabled(hasDocs);
}

void MainWindow::updateWindowMenu()
{
    windowMenu->clear();
    windowMenu->addAction("&Каскадом", this, &MainWindow::cascadeWindows);
    windowMenu->addAction("&Мозаикой", this, &MainWindow::tileWindows);
    windowMenu->addSeparator();
    windowMenu->addAction("С&ледующее", this, &MainWindow::nextWindow);
    windowMenu->addAction("П&редыдущее", this, &MainWindow::previousWindow);
    windowMenu->addSeparator();

    QMdiArea *area = activeArea();
    QList<QMdiSubWindow*> windows = area ? area->subWindowList() : QList<QMdiSubWindow*>();

    for (int i = 0; i < windows.size(); ++i) {
        auto *doc = qobject_cast<BaseDocWidget*>(windows[i]->widget());
        QString text = doc ? QFileInfo(doc->filePath()).baseName()
                           : QString("Документ %1").arg(i + 1);
        QAction *action = windowMenu->addAction(text);
        action->setCheckable(true);
        action->setChecked(windows[i] == (area ? area->activeSubWindow() : nullptr));
        QMdiSubWindow *subWindow = windows[i];
        connect(action, &QAction::triggered, this, [area, subWindow]() {
            if (area) area->setActiveSubWindow(subWindow);
        });
    }
}

void MainWindow::cascadeWindows() { if (QMdiArea *a = activeArea()) a->cascadeSubWindows(); }
void MainWindow::tileWindows()    { if (QMdiArea *a = activeArea()) a->tileSubWindows(); }
void MainWindow::nextWindow()     { if (QMdiArea *a = activeArea()) a->activateNextSubWindow(); }
void MainWindow::previousWindow() { if (QMdiArea *a = activeArea()) a->activatePreviousSubWindow(); }

void MainWindow::aboutApp()
{
    QDialog dlg(this);
    dlg.setWindowTitle("О программе");
    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    QLabel* lblPix = new QLabel(&dlg);
    QString appDir = QCoreApplication::applicationDirPath();
    QPixmap pixmap(appDir + "/images/logotip.bmp");
    if (!pixmap.isNull())
        lblPix->setPixmap(pixmap);

    QLabel* lblText = new QLabel(&dlg);
    lblText->setText("rmo v1.0\nКроссплатформенное MDI-приложение\nAstra Linux / Windows 10/11");

    layout->addWidget(lblPix);
    layout->addWidget(lblText);
    dlg.exec();
}

void MainWindow::onSubWindowActivated(QMdiSubWindow *window)
{
    Q_UNUSED(window);
    updateMenus();
}

void MainWindow::onAuthTimerTimeout()
{
    if (authTimeSeconds > 0) --authTimeSeconds;

    int mins = authTimeSeconds / 60;
    int secs = authTimeSeconds % 60;
    timeLabel->setText(QString("%1:%2")
                       .arg(mins, 2, 10, QChar('0'))
                       .arg(secs, 2, 10, QChar('0')));

    if (authTimeSeconds == 0) {
        authTimer->stop();
        timeLabel->setStyleSheet(
            "font-family: 'Consolas', 'Courier New', monospace; "
            "font-weight: bold; color: #C62828; font-size: 15px; "
            "margin: 0px 8px;");

        // ✅ Логируем автовыход, если был авторизован.
        if (!m_authUser.isEmpty()) {
            DatabaseManager::instance().setCurrentUserName(m_authUser);
            DatabaseManager::instance().logUserAction(
                "Пользователи", QString(),
                QString("Автоматический выход по таймауту: %1").arg(m_authUser));
        }

        m_authUser.clear();

        authComboBox->blockSignals(true);
        authComboBox->setCurrentIndex(-1);
        authComboBox->blockSignals(false);

        // ✅ Возвращаемся к правам смены (или Аноним).
        DatabaseManager::instance().setCurrentUserName(effectiveUserName());
        DatabaseManager::instance().setCurrentUserRole(effectiveUserRole());

        applyUserPermissions();
    }
}

void MainWindow::onLogoutClicked()
{
    authTimer->stop();

    // ✅ Логируем выход под именем авторизованного.
    if (!m_authUser.isEmpty()) {
        DatabaseManager::instance().setCurrentUserName(m_authUser);
        DatabaseManager::instance().logUserAction(
            "Пользователи", QString(),
            QString("Выход из системы (кнопка): %1").arg(m_authUser));
    }

    m_authUser.clear();

    timeLabel->setText("00:00");
    timeLabel->setStyleSheet(
        "font-family: 'Consolas', 'Courier New', monospace; "
        "font-weight: bold; color: #999999; font-size: 15px; "
        "margin: 0px 8px;");

    authComboBox->blockSignals(true);
    authComboBox->setCurrentIndex(-1);
    authComboBox->blockSignals(false);

    // ✅ Возвращаемся к правам смены (или Аноним).
    DatabaseManager::instance().setCurrentUserName(effectiveUserName());
    DatabaseManager::instance().setCurrentUserRole(effectiveUserRole());

    applyUserPermissions();
}

void MainWindow::applyUserPermissions()
{
    const QString authUser  = m_authUser;
    const QString shiftUser = m_shiftUser;

    qDebug() << "[Permissions] authUser:" << authUser
             << "shiftUser:" << shiftUser;

    // ========================================================================
    // Никто не авторизован — всё запрещено.
    // («Смена принял» даёт только имя в журнале, но не права.)
    // ========================================================================
    if (authUser.isEmpty()) {
        newAntAct->setEnabled(false);
        newTxAct->setEnabled(false);
        newModAct->setEnabled(false);
        newEnrAct->setEnabled(false);
        openAct->setEnabled(false);
        saveAct->setEnabled(false);
        saveAsAct->setEnabled(false);
        closeAct->setEnabled(false);
        configAct->setEnabled(false);
        userConfigAct->setEnabled(false);
        changePasswordAct->setEnabled(false);
        equipmentConfigAct->setEnabled(true);
        serverClientConfigAct->setEnabled(false);
        databaseConfigAct->setEnabled(false);
        equipmentSettingsAct->setEnabled(true);
        qDebug() << "[Permissions] Пользователь не авторизован — все права заблокированы";
        return;
    }

    // ========================================================================
    // Авторизованный пользователь — права из БД.
    // ========================================================================
    auto &db = DatabaseManager::instance();

    const bool canCreate     = db.hasPermission(authUser, "create_documents");
    const bool canOpen       = db.hasPermission(authUser, "open_documents");
    const bool canSave       = db.hasPermission(authUser, "save_documents");
    const bool canDelete     = db.hasPermission(authUser, "delete_documents");
    const bool canConfigScr  = db.hasPermission(authUser, "change_screen_config");
    const bool canManageUser = db.hasPermission(authUser, "manage_users");
    const bool canConfigNet  = db.hasPermission(authUser, "change_network_settings");
    const bool canConfigDb   = db.hasPermission(authUser, "change_database_settings");

    newAntAct->setEnabled(canCreate);
    newTxAct->setEnabled(canCreate);
    newModAct->setEnabled(canCreate);
    newEnrAct->setEnabled(canCreate);
    openAct->setEnabled(canOpen);
    saveAct->setEnabled(canSave);
    saveAsAct->setEnabled(canSave);
    closeAct->setEnabled(canDelete);
    configAct->setEnabled(canConfigScr);
    userConfigAct->setEnabled(canManageUser);
    changePasswordAct->setEnabled(true);
    serverClientConfigAct->setEnabled(canConfigNet);
    databaseConfigAct->setEnabled(canConfigDb);
    equipmentSettingsAct->setEnabled(true);
    equipmentConfigAct->setEnabled(true);

    qDebug() << "[Permissions] canCreate:" << canCreate
             << "canOpen:" << canOpen
             << "canSave:" << canSave
             << "canDelete:" << canDelete
             << "canManageUser:" << canManageUser;
}

void MainWindow::showUserManagement()
{
    // ========================================================================
    // 1. Проверка доступа — только авторизованный пользователь с правом
    //    manage_users. Смена без авторизации — не может управлять пользователями.
    // ========================================================================
    if (m_authUser.isEmpty()) {
        QMessageBox::warning(this, "Доступ запрещён",
            "Необходима авторизация для доступа к конфигурации пользователей.");
        return;
    }

    if (!DatabaseManager::instance().hasPermission(m_authUser, "manage_users")) {
        QMessageBox::warning(this, "Доступ запрещён",
            "Только администратор может управлять пользователями.");
        return;
    }

    if (!DatabaseManager::instance().isConnected()) {
        QMessageBox::critical(this, "Ошибка",
            "База данных не подключена.\n"
            "Невозможно открыть конфигурацию пользователей.");
        return;
    }

    // ========================================================================
    // 2. Запоминаем состояние ДО открытия диалога.
    // ========================================================================
    const QString authUserBefore  = m_authUser;
    const QString shiftUserBefore = m_shiftUser;
    const bool    wasAuthenticated = !authUserBefore.isEmpty();
    const bool    hadShift         = !shiftUserBefore.isEmpty();

    // ========================================================================
    // 3. Открываем диалог управления пользователями.
    // ========================================================================
    UserManagementDialog dlg(this);
    dlg.exec();

    // ========================================================================
    // 4. Перечитываем список пользователей из БД.
    // ========================================================================
    QStringList users = DatabaseManager::instance().getUsernames();

    // ========================================================================
    // 5. Обновляем ComboBox «Смену принял».
    // ========================================================================
    if (shiftComboBox) {
        shiftComboBox->blockSignals(true);
        shiftComboBox->clear();
        shiftComboBox->addItems(users);

        if (hadShift) {
            const int newIdx = users.indexOf(shiftUserBefore);
            if (newIdx >= 0) {
                // Пользователь смены ещё существует — восстанавливаем выбор.
                shiftComboBox->setCurrentIndex(newIdx);
                m_shiftUser = shiftUserBefore;
            } else {
                // ✅ Пользователь смены удалён из БД.
                qWarning() << "[Shift] Пользователь" << shiftUserBefore
                           << "удалён из БД — сбрасываем смену";

                shiftComboBox->setCurrentIndex(-1);
                m_shiftUser.clear();
            }
        } else {
            shiftComboBox->setCurrentIndex(-1);
            m_shiftUser.clear();
        }

        shiftComboBox->blockSignals(false);
    }

    // ========================================================================
    // 6. Обновляем ComboBox «Авторизация».
    // ========================================================================
    authComboBox->blockSignals(true);
    authComboBox->clear();
    authComboBox->addItems(users);

    bool authStillValid = false;
    if (wasAuthenticated) {
        const int newIdx = users.indexOf(authUserBefore);
        if (newIdx >= 0) {
            // Пользователь авторизации ещё существует — восстанавливаем выбор.
            authComboBox->setCurrentIndex(newIdx);
            m_authUser = authUserBefore;
            authStillValid = true;
        } else {
            // ✅ Пользователь авторизации удалён из БД.
            qWarning() << "[Auth] Пользователь" << authUserBefore
                       << "удалён из БД — сеанс завершён";

            authComboBox->setCurrentIndex(-1);
            m_authUser.clear();

            // Останавливаем таймер авторизации, сбрасываем индикатор.
            authTimer->stop();
            timeLabel->setText("00:00");
            timeLabel->setStyleSheet(
                "font-family: 'Consolas', 'Courier New', monospace; "
                "font-weight: bold; color: #999999; font-size: 15px; "
                "margin: 0px 8px;");

            QMessageBox::information(this, "Внимание",
                QString("Пользователь '%1' был удалён из базы данных.\n"
                        "Сеанс работы завершён.").arg(authUserBefore));
        }
    } else {
        authComboBox->setCurrentIndex(-1);
        m_authUser.clear();
    }

    authComboBox->blockSignals(false);

    // ========================================================================
    // 7. Обновляем имя и роль для журнала действий.
    //    Приоритет: auth → shift → «Аноним».
    // ========================================================================
    DatabaseManager::instance().setCurrentUserName(effectiveUserName());
    DatabaseManager::instance().setCurrentUserRole(effectiveUserRole());

    // ========================================================================
    // 8. Применяем права заново.
    //    Если пользователь был удалён — права упадут до смены (или пусто).
    // ========================================================================
    applyUserPermissions();

    qDebug() << "[UserManagement] После закрытия диалога:"
             << "authUser:" << m_authUser
             << "shiftUser:" << m_shiftUser
             << "effective:" << effectiveUserName()
             << "role:" << effectiveUserRole()
             << "authStillValid:" << authStillValid;
}

void MainWindow::showChangePassword()
{
    if (m_authUser.isEmpty()) {
        QMessageBox::warning(this, "Доступ запрещён",
            "Для смены пароля необходимо авторизоваться.");
        return;
    }

    QString role = DatabaseManager::instance().getUserRole(m_authUser);
    ChangePasswordDialog dlg(m_authUser, role, this);
    dlg.exec();
}

void MainWindow::showEquipmentSettings()
{
    QString activeFilePath = getActiveDocumentPath();
    if (activeFilePath.isEmpty()) {
        QMessageBox::warning(this, "Нет активного окна",
            "Откройте или создайте документ оборудования,\n"
            "чтобы настроить его параметры.");
        return;
    }

    EquipmentSettingsDialog dlg(this);
    dlg.setDocumentFilePath(activeFilePath);
    dlg.setCurrentUser(m_authUser);
    dlg.exec();
}

void MainWindow::showEquipmentConfig()
{
/*    if (m_authUser.isEmpty()) {
        QMessageBox::warning(this, "Требуется авторизация",
            "Для изменения настроек необходимо авторизоваться.");
        return;
    }*/
    QString activeFilePath = getActiveDocumentPath();
    if (activeFilePath.isEmpty()) {
        QMessageBox::warning(this, "Нет активного окна",
            "Откройте или создайте документ оборудования,\n"
            "чтобы настроить его конфигурацию.");
        return;
    }

    QString ext = QFileInfo(activeFilePath).suffix().toLower();
    DocType docType;
    if (ext == "ant") docType = DocType::Antenna;
    else if (ext == "per") docType = DocType::Transmitter;
    else if (ext == "mod") docType = DocType::Modulator;
    else if (ext == "enr") docType = DocType::Energo;
    else {
        QMessageBox::warning(this, "Ошибка", "Неизвестный тип документа.");
        return;
    }

    EquipmentConfigDialog dlg(this);
    dlg.setDocumentType(docType);
    dlg.setDocumentFilePath(activeFilePath);
    dlg.setCurrentUser(m_authUser);
    dlg.exec();
}

int MainWindow::getEquipmentRowCount(DocType type)
{
    return getEquipmentConnectionNames(type).size();
}

QStringList MainWindow::getEquipmentConnectionNames(DocType type)
{
    switch (type) {
        case DocType::Antenna:
            return QStringList()
                << "Контроллер антенны"
                << "Приёмник пилот-сигнала"
                << "Позиционер азимута"
                << "Позиционер угла места"
                << "Энкодер азимута"
                << "Энкодер угла места";
        case DocType::Transmitter:
            return QStringList()
                << "Усилитель мощности \"А\""
                << "Усилитель мощности \"В\""
                << "Усилитель мощности \"С\""
                << "Переключатель УМ"
                << "UP/C основной (комплект 1)"
                << "UP/C резервный (комплект 1)"
                << "Переключатель UP/C (комплект 1)"
                << "UP/C основной (комплект 2)"
                << "UP/C резервный (комплект 2)"
                << "Переключатель UP/C (комплект 2)"
                << "UP/C основной (комплект 3)"
                << "UP/C резервный (комплект 3)"
                << "Переключатель UP/C (комплект 3)"
                << "Дегидратор"
                << "Температура";
        case DocType::Modulator:
            return QStringList()
                << "Усилитель-распределитель"
                << "Спутниковый модулятор 1"
                << "Спутниковый модулятор 2"
                << "Переключатель резервирования"
                << "Анализатор потока";
        case DocType::Energo:
            return QStringList()
                << "[1] Ввод"
                << "Выход АВР-5"
                << "[2] Ввод"
                << "Выход АВР-6"
                << "[3] Ввод"
                << "Выход АВР-7"
                << "[4] Ввод"
                << "Выход АВР-8"
                << "ЩБП"
                << "Шкаф сигнализации"
                << "ИБП-1"
                << "ИБП-2"
                << "ИБП-3"
                << "Температура";
        default:
            return QStringList() << "";
    }
}

QList<QPair<QString, int>> MainWindow::getEquipmentListForRow(DocType type, int rowIndex)
{
    QList<QPair<QString, int>> result;

    switch (type) {
        case DocType::Antenna:
            switch (rowIndex) {
                case 0:
                    result << qMakePair(QString("Electrospace 93C-23"), ID_ANT_ELECTROSPACE_93C_23);
                    result << qMakePair(QString("Electrospace 93C-23F"), ID_ANT_ELECTROSPACE_93C_23F);
                    result << qMakePair(QString("Vertex 7200 ACS"), ID_ANT_VERTEX_7200_ACS);
                    result << qMakePair(QString("Delta SU5-1"), ID_ANT_DELTA_SU5_1);
                    result << qMakePair(QString("Delta SU12B"), ID_ANT_DELTA_SU12B);
                    result << qMakePair(QString("Delta SU4-8"), ID_ANT_DELTA_SU4_8);
                    result << qMakePair(QString("Vertex 7134 ACS"), ID_ANT_VERTEX_7134_ACS);
                    result << qMakePair(QString("ADAM 5510E"), ID_ANT_ADAM_5510E);
                    result << qMakePair(QString("SIMATIC IPC427C"), ID_ANT_SIMATIC_IPC427C);
                    break;
                case 1:
                    result << qMakePair(QString("Tuner TC-70-04"), ID_PILOT_TUNER_TC_70_04);
                    result << qMakePair(QString("ADAM 4017"), ID_PILOT_ADAM_4017);
                    result << qMakePair(QString("Vertex TRL-2S"), ID_PILOT_VERTEX_TRL_2S);
                    result << qMakePair(QString("Vertex CG-1220"), ID_PILOT_VERTEX_CG_1220);
                    result << qMakePair(QString("ASC300LW"), ID_PILOT_ASC300LW);
                    result << qMakePair(QString("Beacon 3430"), ID_PILOT_BEACON_3430);
                    result << qMakePair(QString("Peak RTR50"), ID_PILOT_PEAK_RTR50);
                    result << qMakePair(QString("iCPCon TM-AD8"), ID_PILOT_ICPCON_TM_AD8);
                    result << qMakePair(QString("Probecom BR3500L"), ID_PILOT_PROBECOM_BR3500L);
                    result << qMakePair(QString("GD DTR"), ID_PILOT_GD_DTR);
                    result << qMakePair(QString("TR PSN"), ID_PILOT_TR_PSN);
                    break;
                case 2:
                case 3:
                    result << qMakePair(QString("MicroMaster 440"), ID_POS_MICROMASTER_440);
                    result << qMakePair(QString("iCPDAS I-87068"), ID_POS_ICPDAS_I_87068);
                    break;
                case 4:
                case 5:
                    result << qMakePair(QString("FRABA OCD-DPB1B"), ID_ENC_FRABA_OCD_DPB1B);
                    break;
                default:
                    result << qMakePair(QString("(Не выбрано)"), ID_UNKNOWN);
                    break;
            }
            break;
        case DocType::Transmitter:
            switch (rowIndex) {
                case 0: // Усилитель мощности "А"
                case 1: // Усилитель мощности "В"
                case 2: // Усилитель мощности "С"
                    result << qMakePair(QString("Advantech ARMAC-250"), ID_TX_ADVANTECH_ARMAC_250);
                    result << qMakePair(QString("Advantech ARMAG-CRL400"), ID_TX_ADVANTECH_ARMAG_CRL400);
                    result << qMakePair(QString("CPI VZU-6997AC"), ID_TX_CPI_VZU_6997AC);
                    result << qMakePair(QString("CPI VZU-6997AF"), ID_TX_CPI_VZU_6997AF);
                    result << qMakePair(QString("CPI TL04CO"), ID_TX_CPI_TL04CO);
                    result << qMakePair(QString("CPI TL05KO"), ID_TX_CPI_TL05KO);
                    result << qMakePair(QString("Maxtech PDC6200N"), ID_TX_MAXTECH_PDC6200N);
                    result << qMakePair(QString("MCL MT3000"), ID_TX_MCL_MT3000);
                    result << qMakePair(QString("MCL MT5000"), ID_TX_MCL_MT5000);
                    result << qMakePair(QString("Paradise HPAC2250BC"), ID_TX_PARADISE_HPAC2250BC);
                    result << qMakePair(QString("Paradise HPAC2600BR"), ID_TX_PARADISE_HPAC2600BR);
                    result << qMakePair(QString("Varian VZC-6965F7"), ID_TX_VARIAN_VZC_6965F7);
                    result << qMakePair(QString("Varian VZU-6995F6"), ID_TX_VARIAN_VZU_6995F6);
                    result << qMakePair(QString("Varian VZU-6994A3"), ID_TX_VARIAN_VZU_6994A3);
                    result << qMakePair(QString("Xicom XTRD-400C"), ID_TX_XICOM_XTRD_400C);
                    result << qMakePair(QString("Xicom XTRD-750C"), ID_TX_XICOM_XTRD_750C);
                    result << qMakePair(QString("Xicom XTRD-750K"), ID_TX_XICOM_XTRD_750K);
                    result << qMakePair(QString("Peak UPC7000"), ID_TX_PEAK_UPC7000);
                    result << qMakePair(QString("Radis UM5765-600P"), ID_TX_RADIS_UM5765_600P);
                    result << qMakePair(QString("Probecom KA 40W BUC"), ID_TX_PROBECOM_40W_KA_BUC);
                    break;
                case 3: // Переключатель усилителей мощности
                    result << qMakePair(QString("Internal Switch"), ID_TX_SW_INTERNAL);
                    result << qMakePair(QString("CPI NCB-1100X"), ID_TX_SW_CPI_NCB_1100X);
                    result << qMakePair(QString("Varian VJC-6767S"), ID_TX_SW_VARIAN_VJC_6767S);
                    result << qMakePair(QString("Varian VZC-6965P6"), ID_TX_SW_VARIAN_VZC_6965P6);
                    result << qMakePair(QString("Xicom XTC-100D"), ID_TX_SW_XICOM_XTC_100D);
                    result << qMakePair(QString("Xicom XTC-114D"), ID_TX_SW_XICOM_XTC_114D);
                    result << qMakePair(QString("MCL Combiner"), ID_TX_SW_MCL_COMBINER);
                    result << qMakePair(QString("CPI 0103233001 (1k2)"), ID_TX_SW_CPI_MODEL_0103233001_1k2);
                    result << qMakePair(QString("CPI 0103233001 (1k1)"), ID_TX_SW_CPI_MODEL_0103233001_1k1);
                    break;
                case 4: // UP/C основной (Комплект 1)
                case 5: // UP/C резервный (Комплект 1)
                case 7: // UP/C основной (Комплект 2)
                case 8: // UP/C резервный (Комплект 2)
                case 10: // UP/C основной (Комплект 3)
                case 11: // UP/C резервный (Комплект 3)
                    result << qMakePair(QString("Comtech UT-4500"), ID_UPC_COMTECH_UT_4500);
                    result << qMakePair(QString("WSI UT-4500"), ID_UPC_WSI_UT_4500);
                    result << qMakePair(QString("Miteq U-9900"), ID_UPC_MITEQ_U_9900);
                    result << qMakePair(QString("Peak IBU655"), ID_UPC_PEAK_IBU655);
                    break;
                case 6: // Переключатель UP/C (Комплект 1)
                case 9: // Переключатель UP/C (Комплект 2)
                case 12: // Переключатель UP/C (Комплект 3)
                    result << qMakePair(QString("Miteq NSU"), ID_UPC_SW_MITEQ_NSU);
                    result << qMakePair(QString("Peak RCU100"), ID_UPC_SW_PEAK_RCU100);
                    result << qMakePair(QString("Advantech ARUD-LCXR"), ID_UPC_SW_ADVANTECH_ARUD_LCXR);
                    result << qMakePair(QString("IRT IFC-L1C210R-RM00"), ID_UPC_SW_IRT_IFC_L1C210R_RM00);
                    break;
                case 13: // дегидратор
                    result << qMakePair(QString("EDH-4"), ID_DEHYD_EDH_4);
                    result << qMakePair(QString("ADH Netcom"), ID_DEHYD_ADH_NETCOM);
                    result << qMakePair(QString("HR150"), ID_DEHYD_HR150);
                    result << qMakePair(QString("ADBKP-2"), ID_DEHYD_ADBKP_2);
                    break;
                case 14: // Температура
                    result << qMakePair(QString("TE Monitor V6"), ID_TEMP_SNS_TE_MONITOR_V6);
                    result << qMakePair(QString("Boreas 3"), ID_TEMP_SNS_BOREAS_3);
                    break;
                default:
                    result << qMakePair(QString("(Не выбрано)"), ID_UNKNOWN);
                    break;
            }
            break;
        case DocType::Modulator:
            switch (rowIndex) {
            case 0: // Усилитель-распределитель
                result << qMakePair(QString("(Не выбран)"), ID_UNKNOWN);
                break;
            case 1: // Спутниковый модулятор 1
            case 2: // Спутниковый модулятор 2
                result << qMakePair(QString("Enensess-XSSR-VYP0-3001"), ID_MOD_ENENSESS_XSSR);
                result << qMakePair(QString("Teleteh TT-MC-72"), ID_MOD_TELETEH_TT_MC_72);
                result << qMakePair(QString("Sencore SDM 989"), ID_MOD_SENCORE_SDM_989);
                result << qMakePair(QString("Newtec M 6100"), ID_MOD_NEWTEC_M_6100);
                result << qMakePair(QString("Newtec Az 110"), ID_MOD_NEWTEC_AZ_110);
                break;
            case 3: // Переключатель резервирования
                result << qMakePair(QString("Newtec SW Az 210"), ID_MOD_SW_NEWTEC_AZ_210);
                result << qMakePair(QString("Dev-SW"), ID_MOD_SW_DEV);
                result << qMakePair(QString("Enensess-Dev-SW"), ID_MOD_SW_ENENSESS);
                result << qMakePair(QString("Newtec SW 0212"), ID_MOD_SW_NEWTEC_0212);
                break;
            case 4: // Анализатор потока
                result << qMakePair(QString("(Не выбрано)"), ID_UNKNOWN);
                break;
            default:
                result << qMakePair(QString("(Не выбрано)"), ID_UNKNOWN);
                break;
            }
            break;
        case DocType::Energo:
            result << qMakePair(QString("(Не выбрано)"), ID_UNKNOWN);
            break;
        default:
            result << qMakePair(QString("(Не выбрано)"), ID_UNKNOWN);
            break;
    }

    return result;
}

QList<EquipmentConnection> MainWindow::getEquipmentConnections(const QString &filePath)
{
    QList<EquipmentConnection> connections;
    QFile file(filePath);

    if (!file.exists()) return connections;

    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject obj = doc.object();
        QJsonArray connectionsArray = obj.value("connections").toArray();

        for (const QJsonValue &val : connectionsArray) {
            QJsonObject connObj = val.toObject();
            EquipmentConnection conn;
            conn.name = connObj.value("name").toString();
            conn.connected = connObj.value("connected").toBool(false);
            conn.address = connObj.value("address").toString();
            conn.equipmentType = connObj.value("equipmentType").toString();
            conn.useStream = connObj.value("useStream").toBool(false);
            connections.append(conn);
        }
        file.close();
    }

    return connections;
}

void MainWindow::saveEquipmentConnections(const QString &filePath, const QList<EquipmentConnection> &connections)
{
    QFile file(filePath);
    QJsonObject obj;

    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        obj = doc.object();
        file.close();
    }

    QJsonArray connectionsArray;
    for (const EquipmentConnection &conn : connections) {
        QJsonObject connObj;
        connObj["name"] = conn.name;
        connObj["connected"] = conn.connected;
        connObj["address"] = conn.address;
        connObj["equipmentType"] = conn.equipmentType;
        connObj["useStream"] = conn.useStream;
        connectionsArray.append(connObj);
    }

    obj["connections"] = connectionsArray;

    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson());
        file.close();
    }
}

void MainWindow::showServerClientConfig()
{
    if (m_authUser.isEmpty()) {
        QMessageBox::warning(this, "Доступ запрещён",
            "Необходима авторизация для изменения конфигурации сервера/клиента.");
        return;
    }

    if (!DatabaseManager::instance().hasPermission(m_authUser, "change_network_settings")) {
        QMessageBox::warning(this, "Доступ запрещён",
            "Только администратор может изменять конфигурацию сервера/клиента.");
        return;
    }

    ServerClientConfigDialog dlg(this);
    dlg.exec();
}

void MainWindow::loadDatabaseConfig()
{
    QString configPath = QCoreApplication::applicationDirPath() + "/rmo_config.json";
    QFile file(configPath);

    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject obj = doc.object();
        QJsonObject dbConfig = obj.value("database").toObject();

        m_dbConfig.host = dbConfig.value("host").toString("localhost");
        m_dbConfig.port = dbConfig.value("port").toInt(5432);
        m_dbConfig.dbName = dbConfig.value("dbName").toString("rmo_db");
        m_dbConfig.user = dbConfig.value("user").toString("postgres");

        QString encryptedPass = dbConfig.value("password").toString();
        if (!encryptedPass.isEmpty()) {
            m_dbConfig.password = CryptoHelper::decrypt(encryptedPass);
        } else {
            m_dbConfig.password = "";
        }
        file.close();

        qDebug() << "[DB Config] Загружены настройки БД:"
                 << m_dbConfig.host << ":" << m_dbConfig.port
                 << "/" << m_dbConfig.dbName;
    } else {
        m_dbConfig.host = "localhost";
        m_dbConfig.port = 5432;
        m_dbConfig.dbName = "rmo_db";
        m_dbConfig.user = "postgres";
        m_dbConfig.password = "12345";
        qDebug() << "[DB Config] Используются настройки по умолчанию";
    }
}

void MainWindow::showDatabaseConfig()
{
    if (m_authUser.isEmpty()) {
        QMessageBox::warning(this, "Доступ запрещён",
            "Необходима авторизация для изменения конфигурации базы данных.");
        return;
    }

    if (!DatabaseManager::instance().hasPermission(m_authUser, "change_database_settings")) {
        QMessageBox::warning(this, "Доступ запрещён",
            "Только администратор может изменять конфигурацию базы данных.");
        return;
    }

    DatabaseConfigDialog dlg(this);
    dlg.exec();
}

void MainWindow::forceAuthenticate(const QString &username)
{
    if (username.isEmpty()) return;

    int index = authComboBox->findText(username);
    if (index < 0) {
        qWarning() << "[MainWindow] Пользователь не найден в ComboBox:" << username;
        return;
    }

    authComboBox->blockSignals(true);
    authComboBox->setCurrentIndex(index);
    authComboBox->blockSignals(false);

    m_authUser = username;

    QString role = DatabaseManager::instance().getUserRole(username);
    DatabaseManager::instance().setCurrentUserRole(role);
    DatabaseManager::instance().setCurrentUserName(username);

    authTimeSeconds = 1800;
    timeLabel->setText("30:00");
    timeLabel->setStyleSheet(
        "font-family: 'Consolas', 'Courier New', monospace; "
        "font-weight: bold; "
        "color: #005A9E; "
        "font-size: 15px; "
        "margin: 0px 8px;"
    );
    authTimer->start(1000);

    applyUserPermissions();

    qDebug() << "[MainWindow] Принудительная авторизация:" << username
             << "роль:" << role;
}

void MainWindow::applyGridLayout(int groupId)
{
    if (groupId < 0 || groupId >= m_groupAreas.size()) return;

    QMdiArea *area = m_groupAreas[groupId];
    if (!area) return;

    const DocGroup &g = config.groups[groupId];
    const int cols = qMax(1, g.columns);
    const int count = g.docPaths.size();
    if (count == 0) return;

    QSize vpSize = area->viewport()->size();
    if (vpSize.width() <= 0 || vpSize.height() <= 0) {
        // Пока QMdiArea не отрисована — откладываем.
        QTimer::singleShot(50, this, [this, groupId]() { applyGridLayout(groupId); });
        return;
    }

    int margin = 4;

    QVector<int> colMaxWidth(cols, 0);
    for (int i = 0; i < count; ++i) {
        BaseDocWidget *doc = findWidgetByPath(g.docPaths.at(i));
        if (doc) {
            int col = i % cols;
            QSize sz = doc->documentFixedSize();
            if (sz.width() > colMaxWidth[col]) {
                colMaxWidth[col] = sz.width();
            }
        }
    }

    QVector<int> rowY;
    int rows = (count + cols - 1) / cols;
    for (int r = 0; r < rows; ++r) {
        int y = margin;
        if (r > 0) {
            int maxH = 0;
            for (int c = 0; c < cols; ++c) {
                int idx = (r - 1) * cols + c;
                if (idx < count) {
                    BaseDocWidget *d = findWidgetByPath(g.docPaths.at(idx));
                    if (d) {
                        int h = d->documentFixedSize().height();
                        if (h > maxH) maxH = h;
                    }
                }
            }
            y = rowY[r - 1] + maxH + margin;
        }
        rowY.append(y);
    }

    QVector<int> columnOffset(cols, 0);
    for (int i = 0; i < count; ++i) {
        const QString &path = g.docPaths.at(i);
        BaseDocWidget *doc = findWidgetByPath(path);
        if (!doc) continue;

        int row = i / cols;
        int col = i % cols;
        QSize sz = doc->documentFixedSize();

        int currentX = margin;
        for (int c = 0; c < col; ++c) {
            currentX += colMaxWidth[c] + margin;
        }
        int currentY = rowY[row] + columnOffset[col];

        for (QMdiSubWindow *sw : area->subWindowList()) {
            if (sw->widget() == doc) {
                sw->setGeometry(currentX, currentY, sz.width(), sz.height());
                sw->raise();
                break;
            }
        }

        if (m_alarmPanels.contains(path)) {
            QMdiSubWindow *panelSub = qobject_cast<QMdiSubWindow*>(m_alarmPanels[path]);
            if (panelSub) {
                int panelHeight = panelSub->height();
                panelSub->setGeometry(
                    currentX,
                    currentY + sz.height(),
                    sz.width(),
                    panelHeight
                );
                panelSub->raise();
                columnOffset[col] += panelHeight;
            }
        }
    }

    area->viewport()->update();
}

void MainWindow::applyGridLayoutAll()
{
    for (int i = 0; i < m_groupAreas.size(); ++i) {
        applyGridLayout(i);
    }
}

void MainWindow::updateAlarmPanelForDoc(const QString &docPath,
                                        const QList<int> &codes,
                                        const QList<int> &statuses,
                                        const QDateTime &cycleTime)
{
    // Обновляем счётчик критических аварий по документу.
    int criticalCount = 0;
    for (int s : statuses) {
        if (s == 1) ++criticalCount;   // 1 = Critical
    }
    if (criticalCount == 0) {
        m_docCriticalCount.remove(docPath);
    } else {
        m_docCriticalCount[docPath] = criticalCount;
    }
    // Пересчитываем счётчик группы этого документа.
    const int groupId = findGroupIdByDocPath(docPath);
    if (groupId >= 0) {
        recalcGroupCriticalCount(groupId);
        updateTabAppearance(groupId);
    }

    if (codes.isEmpty()) {
        removeAlarmPanelForDoc(docPath);
        return;
    }

    BaseDocWidget *doc = findWidgetByPath(docPath);
    if (!doc) return;

    // Находим группу и окно документа.
    if (groupId < 0 || groupId >= m_groupAreas.size()) return;
    QMdiArea *area = m_groupAreas[groupId];
    if (!area) return;

    QMdiSubWindow *docSubWindow = nullptr;
    for (QMdiSubWindow *sw : area->subWindowList()) {
        if (sw->widget() == doc) {
            docSubWindow = sw;
            break;
        }
    }
    if (!docSubWindow) return;

    if (!m_alarmPanels.contains(docPath)) {
        AlarmPanel *panel = new AlarmPanel(area);
        FixedSubWindow *panelSub = new FixedSubWindow(panel, area);
        panelSub->setAttribute(Qt::WA_DeleteOnClose);
        m_alarmPanels[docPath] = panelSub;

        // подключаем подтверждение аварии к документу.
        //    Пока логика в BaseDocWidget не реализована, лог подтвердит,
        //    что сигнал доходит.
        connect(panel, &AlarmPanel::alarmAcknowledged,
                doc, &BaseDocWidget::onAlarmAcknowledged);

        QPointer<MainWindow> safeThis(this);
        connect(panelSub, &QMdiSubWindow::destroyed, this, [safeThis, docPath]() {
            if (!safeThis) return;
            safeThis->m_alarmPanels.remove(docPath);
        });
    }

    QMdiSubWindow *panelSub = qobject_cast<QMdiSubWindow*>(m_alarmPanels[docPath]);
    if (panelSub) {
        AlarmPanel *panel = qobject_cast<AlarmPanel*>(panelSub->widget());
        if (panel) {
        panel->setAlarms(codes, statuses, cycleTime);
        int docWidth = docSubWindow->width();
            int panelHeight = panel->calculateHeight();
            panelSub->setFixedSize(docWidth, panelHeight);
            QRect docRect = docSubWindow->geometry();
            panelSub->setGeometry(
                docRect.x(),
                docRect.y() + docRect.height(),
                docWidth,
                panelHeight
            );
            panelSub->show();
            panelSub->raise();
        }
    }

    applyGridLayout(groupId);
}

void MainWindow::removeAlarmPanelForDoc(const QString &docPath)
{
    // Убираем документ из счётчиков.
    m_docCriticalCount.remove(docPath);
    const int groupId = findGroupIdByDocPath(docPath);
    if (groupId >= 0) {
        recalcGroupCriticalCount(groupId);
        updateTabAppearance(groupId);
    }

    if (m_alarmPanels.contains(docPath)) {
        QMdiSubWindow *panelSub = qobject_cast<QMdiSubWindow*>(m_alarmPanels[docPath]);
        if (panelSub) panelSub->close();
        m_alarmPanels.remove(docPath);

        const int groupId = findGroupIdByDocPath(docPath);
        if (groupId >= 0) applyGridLayout(groupId);
    }
}


void MainWindow::onViewDataAction()
{
    BaseDocWidget *currentDoc = getCurrentDocument();
    if (!currentDoc) {
        QMessageBox::warning(this, "Ошибка", "Нет активного документа");
        return;
    }

    int docType = 0;
    switch (currentDoc->docType()) {
        case DocType::Antenna:     docType = 1; break;
        case DocType::Transmitter: docType = 2; break;
        case DocType::Energo:      docType = 3; break;
        case DocType::Modulator:   docType = 4; break;
        default: return;
    }

    // ✅ Модальный диалог
    DataViewDialog dlg(currentDoc->filePath(), docType, this);
    dlg.exec();
}


void MainWindow::showUserActionLog()
{
    UserActionLogDialog dlg(this);
    dlg.exec();
}

void MainWindow::showDriverConfigEditor()
{
    if (m_authUser.isEmpty()) {
        QMessageBox::warning(this, "Доступ запрещён",
            "Необходима авторизация для доступа к конфигам драйверов.");
        return;
    }
    if (!DatabaseManager::instance().hasPermission(m_authUser, "edit_driver_config")) {
        QMessageBox::warning(this, "Доступ запрещён",
            "Только администратор может редактировать конфиги драйверов.");
        return;
    }

    DriverConfigEditorDialog dlg(this);
    dlg.exec();
}

QMdiArea* MainWindow::activeArea() const
{
    if (!m_groupTabs) return nullptr;
    const int i = m_groupTabs->currentIndex();
    if (i < 0 || i >= m_groupAreas.size()) return nullptr;
    return m_groupAreas[i];
}

int MainWindow::findGroupIdByDocPath(const QString &path) const
{
    const QString clean = QDir::cleanPath(QDir::toNativeSeparators(path));
    for (int i = 0; i < config.groups.size(); ++i) {
        for (const QString &p : config.groups[i].docPaths) {
            if (QDir::cleanPath(QDir::toNativeSeparators(p)) == clean) {
                return i;
            }
        }
    }
    return -1;
}

void MainWindow::onTabBlinkTimeout()
{
    if (!m_groupTabs) return;
    m_groupTabs->setBlinkState(!m_groupTabs->isBlinkOn());
}

void MainWindow::updateTabAppearance(int groupId)
{
    Q_UNUSED(groupId);
    if (!m_groupTabs) return;

    if (!m_groupTabs->isVisible()) {
        m_groupTabs->setCriticalGroups({});
        return;
    }

    QSet<int> critical;
    for (int i = 0; i < m_groupCriticalCount.size(); ++i) {
        if (m_groupCriticalCount[i] > 0) critical.insert(i);
    }
    m_groupTabs->setCriticalGroups(critical);
}

void MainWindow::moveDocumentToGroup(const QString &docPath, int newGroupId)
{
    const int oldGroupId = findGroupIdByDocPath(docPath);
    if (oldGroupId < 0) {
        qWarning() << "[MainWindow] moveDocumentToGroup: документ не найден:" << docPath;
        return;
    }
    if (oldGroupId == newGroupId) return;

    if (newGroupId < 0 || newGroupId >= m_groupAreas.size()) {
        qWarning() << "[MainWindow] moveDocumentToGroup: неверный newGroupId:" << newGroupId;
        return;
    }

    QMdiArea *oldArea = m_groupAreas[oldGroupId];
    QMdiArea *newArea = m_groupAreas[newGroupId];

    // 1. Находим FixedSubWindow в старой группе.
    BaseDocWidget *doc = nullptr;
    FixedSubWindow *oldSub = nullptr;
    for (QMdiSubWindow *sw : oldArea->subWindowList()) {
        auto *fixed = qobject_cast<FixedSubWindow*>(sw);
        if (fixed && fixed->widget()) {
            auto *w = qobject_cast<BaseDocWidget*>(fixed->widget());
            if (w && w->filePath() == docPath) {
                doc = w;
                oldSub = fixed;
                break;
            }
        }
    }

    if (!doc || !oldSub) {
        qWarning() << "[MainWindow] moveDocumentToGroup: подокно не найдено для:" << docPath;
        return;
    }

    // 2. Удаляем панель аварий (в старой группе). В новой пересоздастся.
    removeAlarmPanelForDoc(docPath);

    // 3. Отвязываем виджет от подокна, чтобы QMdiSubWindow не удалил его.
    oldSub->setWidget(nullptr);

    // 3.5. Отключаем старый обработчик destroyed, чтобы при удалении oldSub
    //      документ не был удалён из конфига.
    disconnect(oldSub, &QMdiSubWindow::destroyed, nullptr, nullptr);

    // 4. Удаляем старое подокно.
    oldSub->deleteLater();

    // 5. Создаём новое подокно в новой группе.
    FixedSubWindow *newSub = new FixedSubWindow(doc, newArea);
    newSub->setGroupId(newGroupId);
    newSub->setAttribute(Qt::WA_DeleteOnClose);
    newSub->setFixedSize(doc->documentFixedSize());
    newSub->show();

    // 6. Обработчик destroyed — привязываем заново на новое подокно.
    //    Старый обработчик привязан к oldSub — он удалится вместе с oldSub,
    //    но нам нужен новый для newSub.
    QString cleanPath = QDir::cleanPath(QDir::toNativeSeparators(docPath));
    QPointer<MainWindow> safeThis(this);
    connect(newSub, &QMdiSubWindow::destroyed, this, [safeThis, cleanPath]() {
        if (!safeThis) return;
        QTimer::singleShot(0, safeThis.data(), [safeThis, cleanPath]() {
            if (!safeThis) return;
            safeThis->removeDocumentPath(cleanPath);
            safeThis->applyGridLayoutAll();
            if (safeThis->m_mode == AppMode::Server) {
                safeThis->broadcastConfigToClients();
            }
        });
    });

    // 7. Обновляем конфиг: убираем из старой группы, добавляем в новую.
    config.groups[oldGroupId].docPaths.removeAll(docPath);
    if (!config.groups[newGroupId].docPaths.contains(docPath)) {
        config.groups[newGroupId].docPaths.append(docPath);
    }

    // 8. Пересчитываем layout обеих групп.
    applyGridLayout(oldGroupId);
    applyGridLayout(newGroupId);
}

void MainWindow::applyScreenConfig(const QList<DocGroup> &newGroups)
{
    if (newGroups.isEmpty()) {
        qWarning() << "[MainWindow] applyScreenConfig: пустой список групп";
        return;
    }

    qDebug() << "[applyScreenConfig] Начало. Групп:" << newGroups.size();
    for (const DocGroup &g : newGroups) {
        qDebug() << "  group" << g.name
                 << "columns=" << g.columns
                 << "docs=" << g.docPaths.size();
    }

    const QList<DocGroup> oldGroups = config.groups;

    QMap<QString, int> oldIndexByName;
    for (int i = 0; i < oldGroups.size(); ++i) {
        oldIndexByName[oldGroups[i].name] = i;
    }
    QMap<QString, int> newIndexByName;
    for (int i = 0; i < newGroups.size(); ++i) {
        newIndexByName[newGroups[i].name] = i;
    }

    // ==================================================================
    // ШАГ 1. Определить физические переезды документов.
    //   Для каждого документа в новой структуре — найти его старую группу
    //   и сравнить с новой. Если разные — переехать.
    // ==================================================================
    struct Move { QString path; int oldGroup; int newGroup; };
    QList<Move> moves;

    // Собираем карту: path → oldGroupId (по oldGroups).
    QMap<QString, int> docOldGroup;
    for (int i = 0; i < oldGroups.size(); ++i) {
        for (const QString &path : oldGroups[i].docPaths) {
            docOldGroup[path] = i;
        }
    }

    // Для каждого документа в новой структуре — где он был и куда переехал.
    for (int newIdx = 0; newIdx < newGroups.size(); ++newIdx) {
        for (const QString &path : newGroups[newIdx].docPaths) {
            if (!docOldGroup.contains(path)) {
                // Новый документ (не должно быть — диалог не добавляет документы).
                continue;
            }
            int oldIdx = docOldGroup[path];
            if (oldIdx == newIdx) continue;

            // Проверяем: старая группа и новая группа — это ОДНА и та же
            // физическая группа (по имени)? Если имена совпадают — это не
            // переезд, а просто изменение индекса из-за перестановки групп.
            const QString &oldName = oldGroups[oldIdx].name;
            const QString &newName = newGroups[newIdx].name;
            if (oldName == newName) {
                // Та же группа, только индекс сдвинулся. Это не переезд.
                continue;
            }

            moves.append({ path, oldIdx, newIdx });
        }
    }

    qDebug() << "[applyScreenConfig] Физических переездов:" << moves.size();

    // ==================================================================
    // ШАГ 2. Выполнить физические переезды (QMdiSubWindow между QMdiArea).
    //   Используем moveDocumentToGroup, но он модифицирует config.groups —
    //   это не страшно, config.groups будет перезаписан на шаге 5.
    // ==================================================================
    for (const Move &m : moves) {
        // moveDocumentToGroup ищет группу через findGroupIdByDocPath,
        // то есть через config.groups. Старая группа в config.groups
        // соответствует oldGroups (потому что config.groups ещё не менялся).
        qDebug() << "[applyScreenConfig] Переезд:" << m.path
                 << "из" << m.oldGroup << "в" << m.newGroup;
        moveDocumentToGroup(m.path, m.newGroup);
    }

    // ==================================================================
    // ШАГ 3. Удалить исчезнувшие группы (по имени).
    //   Их документы (если были) уже переехали на шаге 2.
    //   Но на всякий случай: если группа не пуста, не удаляем.
    // ==================================================================
    for (int i = m_groupAreas.size() - 1; i >= 0; --i) {
        const QString &oldName = config.groups[i].name;
        if (newIndexByName.contains(oldName)) continue;

        QMdiArea *area = m_groupAreas[i];
        if (area && !area->subWindowList().isEmpty()) {
            qWarning() << "[applyScreenConfig] Не удаляем непустую группу:"
                       << oldName;
            continue;
        }

        qDebug() << "[applyScreenConfig] Удаление группы:" << oldName;

        m_groupStack->removeWidget(area);
        if (area) area->deleteLater();

        m_groupAreas.removeAt(i);
        m_groupCriticalCount.removeAt(i);
        m_groupTabs->removeTab(i);
        config.groups.removeAt(i);
    }

    // ==================================================================
    // ШАГ 4. Создать новые группы (по имени).
    // ==================================================================
    for (int i = 0; i < newGroups.size(); ++i) {
        const QString &name = newGroups[i].name;

        bool exists = false;
        for (const DocGroup &g : config.groups) {
            if (g.name == name) { exists = true; break; }
        }
        if (exists) continue;

        qDebug() << "[applyScreenConfig] Создание группы:" << name;

        QMdiArea *area = new QMdiArea(this);
        area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        area->setOption(QMdiArea::DontMaximizeSubWindowOnActivation, true);

        m_groupStack->addWidget(area);
        m_groupAreas.append(area);
        m_groupCriticalCount.append(0);
        m_groupTabs->addTab(name);

        connect(area, &QMdiArea::subWindowActivated,
                this, &MainWindow::updateWindowTitle);

        DocGroup g;
        g.name = name;
        g.columns = qMax(1, newGroups[i].columns);
        config.groups.append(g);
    }

    // ==================================================================
    // ШАГ 5. Перестроить config.groups из newGroups.
    //   Здесь документы и columns берутся ИЗ newGroups — это то, что
    //   пользователь видел в диалоге. Порядок и содержимое — финальные.
    // ==================================================================
    QList<DocGroup> finalGroups;
    for (int i = 0; i < newGroups.size(); ++i) {
        DocGroup g;
        g.name = newGroups[i].name;
        g.columns = qMax(1, newGroups[i].columns);
        g.docPaths = newGroups[i].docPaths;      // ← ВОТ ГЛАВНОЕ ИСПРАВЛЕНИЕ
        finalGroups.append(g);
    }
    config.groups = finalGroups;

    // ==================================================================
    // ШАГ 6. Переставить QMdiArea в порядок newGroups.
    // ==================================================================
    QList<QMdiArea*> newOrder;
    QList<int> newCriticalOrder;
    for (int i = 0; i < newGroups.size(); ++i) {
        const QString &name = newGroups[i].name;
        QMdiArea *area = nullptr;
        int crit = 0;
        for (int j = 0; j < m_groupAreas.size(); ++j) {
            if (config.groups[j].name == name) {   // config.groups уже = finalGroups
                area = m_groupAreas[j];
                crit = m_groupCriticalCount[j];
                break;
            }
        }
        if (!area) {
            qWarning() << "[applyScreenConfig] Не нашли QMdiArea для группы:" << name;
            area = new QMdiArea(this);
            area->setOption(QMdiArea::DontMaximizeSubWindowOnActivation, true);
            m_groupStack->addWidget(area);
            connect(area, &QMdiArea::subWindowActivated,
                    this, &MainWindow::updateWindowTitle);
        }
        newOrder.append(area);
        newCriticalOrder.append(crit);
    }
    m_groupAreas = newOrder;
    m_groupCriticalCount = newCriticalOrder;

    // ==================================================================
    // ШАГ 7. Пересобрать QTabBar.
    // ==================================================================
    m_groupTabs->blockSignals(true);
    while (m_groupTabs->count() > 0) m_groupTabs->removeTab(0);
    for (const DocGroup &g : config.groups) {
        m_groupTabs->addTab(g.name);
    }
    m_groupTabs->blockSignals(false);
    // обновляем видимость
    updateGroupTabsVisibility();

    // ✅ Пересчитываем счётчики для всех групп.
    for (int i = 0; i < config.groups.size(); ++i) {
        recalcGroupCriticalCount(i);
    }
    for (int i = 0; i < m_groupAreas.size(); ++i) {
        updateTabAppearance(i);
    }
    // ==================================================================
    // ШАГ 8. Пересчитать layout для всех групп — применит новый порядок
    //   документов и новые columns.
    // ==================================================================
    applyGridLayoutAll();

    // ==================================================================
    // ШАГ 9. Сохранить и разослать.
    // ==================================================================
    saveConfig();
    if (m_mode == AppMode::Server) {
        broadcastConfigToClients();
    }

    qDebug() << "[applyScreenConfig] Завершено";
}

// mainwindow.cpp
void MainWindow::updateGroupTabsVisibility()
{
    if (!m_groupTabs) return;
    m_groupTabs->setVisible(m_groupTabs->count() > 1);
}

void MainWindow::recalcGroupCriticalCount(int groupId)
{
    if (groupId < 0 || groupId >= config.groups.size()) return;
    if (groupId >= m_groupCriticalCount.size()) return;

    const DocGroup &g = config.groups[groupId];
    int total = 0;
    for (const QString &path : g.docPaths) {
        total += m_docCriticalCount.value(path, 0);
    }
    m_groupCriticalCount[groupId] = total;
}

void MainWindow::onShiftComboBoxChanged(int index)
{
    // ----------------------------------------------------------------
    // Снятие выбора: «смену сдал».
    // ----------------------------------------------------------------
    if (index < 0) {
        if (m_shiftUser.isEmpty()) return;

        // Логируем «смену сдал» под именем предыдущего.
        DatabaseManager::instance().setCurrentUserName(m_shiftUser);
        DatabaseManager::instance().logUserAction(
            "Смена", QString(),
            QString("Смена сдана: %1").arg(m_shiftUser));

        qDebug() << "[Shift] Смена сдана:" << m_shiftUser;
        m_shiftUser.clear();

        // Если пользователь не авторизован — имя для журнала станет «Аноним».
        DatabaseManager::instance().setCurrentUserName(effectiveUserName());
        DatabaseManager::instance().setCurrentUserRole(effectiveUserRole());

        applyUserPermissions();
        return;
    }

    // ----------------------------------------------------------------
    // Выбор пользователя: «смену принял» (с паролем).
    // ----------------------------------------------------------------
    const QString username = shiftComboBox->currentText();
    if (username.isEmpty()) return;

    // ✅ Запрашиваем пароль.
    AuthDialog authDlg(username, this);
    if (authDlg.exec() != QDialog::Accepted || !authDlg.isAuthenticated()) {
        // Отмена или неверный пароль — возвращаем ComboBox в предыдущее состояние.
        shiftComboBox->blockSignals(true);
        shiftComboBox->setCurrentIndex(
            m_shiftUser.isEmpty() ? -1
                                  : shiftComboBox->findText(m_shiftUser));
        shiftComboBox->blockSignals(false);
        return;
    }

    m_shiftUser = username;

    // ✅ Имя для журнала — по приоритету auth > shift > Аноним.
    DatabaseManager::instance().setCurrentUserName(effectiveUserName());
    DatabaseManager::instance().setCurrentUserRole(effectiveUserRole());

    DatabaseManager::instance().logUserAction(
        "Смена", QString(),
        QString("Смена принята: %1").arg(username));

    qDebug() << "[Shift] Смена принята:" << username;
    applyUserPermissions();
}

QString MainWindow::effectiveUserRole() const
{
    if (!m_authUser.isEmpty()) {
        return DatabaseManager::instance().getUserRole(m_authUser);
    }
    return QString();
}

QString MainWindow::getCurrentUser() const
{
    return effectiveUserName();
}






