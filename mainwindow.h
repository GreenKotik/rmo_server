#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QLabel>
#include <QComboBox>
#include <QToolButton>
#include <QTimer>
#include <QMessageBox>
#include <QJsonDocument>
#include <QStringList>
#include <QMouseEvent>
#include <QSize>
#include <QSizePolicy>
#include <QHBoxLayout>
#include <QPalette>
#include <QPair>
#include <QPointer>
#include "deviceids.h"
#include "basedocwidget.h"
#include "ledindicator.h"
#include "alarmpanel.h"
#include <QPushButton>
#include <QPainter>
#include <QPixmap>
#include <QTabBar>
#include <QStackedWidget>
#include <QProxyStyle>
#include <QTabBar>
#include <QStyleOptionTab>
#include <QPainter>
#include <QSet>
#include <QDateTime>

// ============================================================================
// ✅ 1. Сначала DocGroup — от него зависят ScreenConfig.
// ============================================================================
struct DocGroup {
    QString name;
    int columns = 2;
    QStringList docPaths;
};

// ============================================================================
// ✅ Конфигурация экрана: список групп + глобальные параметры.
// ============================================================================
struct ScreenConfig {
    int columns = 2;            // fallback (не используется, если у всех групп заданы свои)
    QList<DocGroup> groups;     // список групп
};


// ============================================================================
// ✅ QTabBar с поддержкой «аварийных» вкладок (мигающий красный фон).
// ============================================================================
class AlarmTabBar : public QTabBar
{
    Q_OBJECT
public:
    using QTabBar::QTabBar;

    void setCriticalGroups(const QSet<int> &groups) {
        m_criticalGroups = groups;
        update();
    }

    void setBlinkState(bool on) {
        if (m_blinkState != on) {
            m_blinkState = on;
            update();
        }
    }

    bool isBlinkOn() const { return m_blinkState; }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QTabBar::paintEvent(event);

        if (!m_blinkState || m_criticalGroups.isEmpty()) return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        for (int idx : m_criticalGroups) {
            if (idx < 0 || idx >= count()) continue;
            QRect r = tabRect(idx);
            if (!r.isValid()) continue;

            painter.setBrush(QColor(211, 47, 47, 100));
            painter.setPen(QPen(QColor(211, 47, 47), 2));
            painter.drawRoundedRect(r.adjusted(1, 1, -1, -1), 3, 3);
        }
    }

private:
    QSet<int> m_criticalGroups;
    bool m_blinkState = false;
};



class CustomTitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit CustomTitleBar(const QString &title, QWidget *parent = nullptr) : QWidget(parent),
        m_soundOn(true)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 12, 0);
        layout->setSpacing(5);

        titleLabel = new QLabel(title, this);
        titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        titleLabel->setStyleSheet(
            "color: white; "
            "font-weight: bold; "
            "font-size: 14px; "
            "padding-left: 10px;"
        );

        soundButton = new QPushButton(this);
        soundButton->setFixedSize(22, 22);
        soundButton->setIcon(createSpeakerIcon(true));
        soundButton->setIconSize(QSize(20, 20));
        soundButton->setStyleSheet(
            "QPushButton { "
            "  background-color: rgba(255, 255, 255, 30); "
            "  border: 1px solid rgba(255, 255, 255, 50); "
            "  border-radius: 3px; "
            "} "
            "QPushButton:hover { "
            "  background-color: rgba(255, 255, 255, 60); "
            "} "
            "QPushButton:pressed { "
            "  background-color: rgba(255, 255, 255, 90); "
            "}"
        );
        soundButton->setToolTip("Переключить звук");
        connect(soundButton, &QPushButton::clicked, this, &CustomTitleBar::toggleSound);

        indicator = new LedIndicator(this);
        indicator->setFixedSize(16, 16);
        indicator->setState(LedIndicator::State::Gray);
        indicator->setStyleSheet("margin-right: 8px;");

        layout->addWidget(titleLabel);
        layout->addWidget(soundButton);
        layout->addWidget(indicator);

        setMinimumHeight(28);
    }

    void setTitle(const QString &text) { titleLabel->setText(text); }
    LedIndicator* getIndicator() const { return indicator; }

    bool isSoundOn() const { return m_soundOn; }
    void setSoundOn(bool on) {
        if (m_soundOn != on) {
            m_soundOn = on;
            soundButton->setIcon(createSpeakerIcon(m_soundOn));
            emit soundStateChanged(m_soundOn);
        }
    }

    void setBackgroundColor(const QColor &color) {
        if (m_backgroundColor != color) {
            m_backgroundColor = color;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.fillRect(rect(), m_backgroundColor);
        QWidget::paintEvent(event);
    }

signals:
    void soundStateChanged(bool enabled);

private slots:
    void toggleSound() {
        m_soundOn = !m_soundOn;
        soundButton->setIcon(createSpeakerIcon(m_soundOn));
        emit soundStateChanged(m_soundOn);
    }

private:
    QLabel *titleLabel;
    QPushButton *soundButton;
    LedIndicator *indicator;
    bool m_soundOn;
    QColor m_backgroundColor = QColor("#005A9E");

    static QIcon createSpeakerIcon(bool enabled)
    {
        QPixmap pixmap(20, 20);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        QPen pen(Qt::white, 1.5);
        p.setPen(pen);
        p.setBrush(Qt::white);

        QPolygon speaker;
        speaker << QPoint(3, 8)
                << QPoint(7, 8)
                << QPoint(11, 4)
                << QPoint(11, 16)
                << QPoint(7, 12)
                << QPoint(3, 12);
        p.drawPolygon(speaker);

        if (enabled) {
            p.setBrush(Qt::NoBrush);
            p.drawArc(12, 6, 4, 8, -60 * 16, 120 * 16);
            p.drawArc(14, 3, 6, 14, -60 * 16, 120 * 16);
        } else {
            p.setPen(QPen(QColor(255, 100, 100), 2));
            p.drawLine(13, 6, 19, 14);
            p.drawLine(19, 6, 13, 14);
        }
        p.end();
        return QIcon(pixmap);
    }
};

class FixedSubWindow : public QMdiSubWindow {
    Q_OBJECT        // ✅ ДОБАВИТЬ
public:
    explicit FixedSubWindow(QWidget *widget = nullptr, QWidget *parent = nullptr)
        : QMdiSubWindow(parent)
    {
        setWidget(widget);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setWindowFlags(Qt::FramelessWindowHint | Qt::MSWindowsFixedSizeDialogHint);
    }

    int groupId() const { return m_groupId; }
    void setGroupId(int id) { m_groupId = id; }

private:
    int m_groupId = -1;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AppMode mode, const QJsonObject &clientConfig = QJsonObject(), QWidget *parent = nullptr);
    ~MainWindow();

    QList<QPair<QString, int>> getEquipmentListForRow(DocType type, int rowIndex);
    int getEquipmentRowCount(DocType type);
    QStringList getEquipmentConnectionNames(DocType type);
    QString getCurrentUser() const;
    void forceAuthenticate(const QString &username);
    // ✅ НОВОЕ: Получить текущий активный документ
    BaseDocWidget* getCurrentDocument() const;
    QStringList getDocPaths() const;

private slots:
    void newAntenna();
    void newTransmitter();
    void newModulator();
    void newEnergo();
    void openDocument();
    void saveDocument();
    void saveAsDocument();
    void closeDocument();
    void showScreenConfig();
    void updateMenus();
    void updateWindowMenu();
    void cascadeWindows();
    void tileWindows();
    void nextWindow();
    void previousWindow();
    void aboutApp();
    void onSubWindowActivated(QMdiSubWindow *window);
    void onAuthTimerTimeout();
    void onLogoutClicked();
    void showUserManagement();
    void showChangePassword();
    void showEquipmentSettings();
    void showEquipmentConfig();
    void showServerClientConfig();
    void updateWindowTitle();
    void showDatabaseConfig();
    void onViewDataAction();
    void showUserActionLog();
    void onShiftComboBoxChanged(int index);
    void showDriverConfigEditor();

private:
    void setupActions();
    void applyUserPermissions();
    void loadConfig();
    void saveConfig();
    QString getAppDir() const;
    QString getConfigPath() const;
    void addDocumentPath(const QString &path, int groupId);
    void removeDocumentPath(const QString &path);
    QString getActiveDocumentPath();
    void initializeDocumentFile(const QString &filePath, DocType type);
    QString docTypeToString(DocType type);
    QList<EquipmentConnection> getEquipmentConnections(const QString &filePath);
    void saveEquipmentConnections(const QString &filePath, const QList<EquipmentConnection> &connections);
    BaseDocWidget* createDocumentFromFile(const QString &path = QString(),
                                          DocType type = DocType::Antenna,
                                          int groupId = -1,
                                          bool addToConfig = true);
    void applyGridLayout(int groupId);
    void applyGridLayoutAll();   // удобный обход всех групп
    BaseDocWidget* findWidgetByPath(const QString &path) const;
    void openDocumentFromConfig(const QString &path, DocType type, quint16 docPort);
    void loadAppConfig();
    void updateAlarmPanelForDoc(const QString &docPath,
                                const QList<int> &codes,
                                const QList<int> &statuses,
                                const QDateTime &cycleTime);
    void removeAlarmPanelForDoc(const QString &docPath);
    void broadcastConfigToClients();
    void moveDocumentToGroup(const QString &docPath, int newGroupId);
    void applyScreenConfig(const QList<DocGroup> &newGroups);
    void updateGroupTabsVisibility();
    // Критические аварии по документам (для подсчёта по группам).
    QMap<QString, int> m_docCriticalCount;
    // Пересчитать m_groupCriticalCount[groupId] по документам группы.
    void recalcGroupCriticalCount(int groupId);
    // Применить прокси-стиль для мигающих вкладок.
    void installTabBarStyle();

    // ============================================================================
    // ✅ Группы документов (вкладки)
    // ============================================================================

    // Вкладки групп сверху
    AlarmTabBar *m_groupTabs = nullptr;
    // Стек страниц: каждая страница — QMdiArea своей группы
    QStackedWidget *m_groupStack = nullptr;

    // QMdiArea по группам (индекс = индекс группы в config.groups)
    QList<QMdiArea*> m_groupAreas;

    // Количество критических аварий по каждой группе (для окраски вкладок)
    QList<int> m_groupCriticalCount;

    // Таймер мигания «аварийных» вкладок
    QTimer *m_tabBlinkTimer = nullptr;
    // Активная QMdiArea (текущей группы)
    QMdiArea* activeArea() const;
    // Найти группу по пути документа (-1, если не найдено)
    int findGroupIdByDocPath(const QString &path) const;
    // Обновить цвет/мигание вкладки группы
    void updateTabAppearance(int groupId);
    void onTabBlinkTimeout();



    QMenu *fileMenu = nullptr;
    QMenu *windowMenu = nullptr;
    QToolBar *mainToolBar = nullptr;
    QAction *saveAct = nullptr;
    QAction *saveAsAct = nullptr;
    QAction *closeAct = nullptr;
    QAction *newAntAct = nullptr;
    QAction *newTxAct = nullptr;
    QAction *newModAct = nullptr;
    QAction *newEnrAct = nullptr;
    QAction *openAct = nullptr;
    QAction *configAct = nullptr;
    QAction *userConfigAct = nullptr;
    QAction *changePasswordAct = nullptr;
    QAction *equipmentSettingsAct = nullptr;
    QAction *equipmentConfigAct = nullptr;
    QAction *serverClientConfigAct = nullptr;
    QString m_programName;
    QAction *databaseConfigAct = nullptr;
    QAction *viewDataAct = nullptr;   // ✅ НОВЫЙ: Просмотр графика

    struct DatabaseConfig {
        QString host = "localhost";
        int port = 5432;
        QString dbName = "rmo_db";
        QString user = "postgres";
        QString password;
    };
    DatabaseConfig m_dbConfig;
    void loadDatabaseConfig();

    QLabel *authLabel;
    QComboBox *authComboBox;
    QLabel *timeLabel;
    QToolButton *logoutButton;
    QTimer *authTimer;
    int authTimeSeconds;
    ScreenConfig config;
    NetworkController *m_net;
    AppMode m_mode;
    QMap<QString, QWidget*> m_alarmPanels;
    QComboBox *shiftComboBox = nullptr;
    QLabel *shiftLabel = nullptr;

    // ✅ Пользователь, принявший смену (физически за пультом).
    //    Требует пароля при выборе. Даёт базовые права оператора.
    QString m_shiftUser;

    // ✅ Полностью авторизованный пользователь (ввёл пароль).
    //    Приоритетнее m_shiftUser.
    QString m_authUser;

    // ✅ Действующее имя для журнала.
    //    auth → shift → «Аноним» (пустая строка).
    QString effectiveUserName() const {
        if (!m_authUser.isEmpty())  return m_authUser;
        if (!m_shiftUser.isEmpty()) return m_shiftUser;
        return QString();   // → DatabaseManager подставит «Аноним»
    }
    // ✅ Действующая роль.
    QString effectiveUserRole() const;
};

#endif // MAINWINDOW_H
