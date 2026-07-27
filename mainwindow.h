#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QAction>
#include <QToolBar> // ⬅️ ДОБАВИТЬ
#include <QLabel>          // ⬅️ ДОБАВИТЬ
#include <QComboBox>       // ⬅️ ДОБАВИТЬ
#include <QToolButton>     // ⬅️ ДОБАВИТЬ
#include <QTimer>          // ⬅️ ДОБАВИТЬ
#include <QMessageBox>     // ⬅️ ДОБАВИТЬ
#include <QJsonDocument>
#include <QStringList>
#include <QMouseEvent>
#include <QSize>
#include <QSizePolicy>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPair>
#include "deviceids.h"  // ✅ Коды типов оборудования
#include "basedocwidget.h"
#include "ledindicator.h" // Подключаем наш индикатор
#include "alarmpanel.h"

#include <QPushButton>
#include <QPainter>
#include <QPixmap>


struct ScreenConfig {
    int columns = 2;
    QStringList docPaths;
};


class CustomTitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit CustomTitleBar(const QString &title, QWidget *parent = nullptr) : QWidget(parent),
        m_soundOn(true) // По умолчанию звук включен
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

        // ✅ НОВАЯ КНОПКА ЗВУКА
        soundButton = new QPushButton(this);
        soundButton->setFixedSize(22, 22); // Квадратная
        soundButton->setIcon(createSpeakerIcon(true));
        soundButton->setIconSize(QSize(20, 20));
        // Убираем рамку, чтобы кнопка сливалась с заголовком
        soundButton->setStyleSheet(
        "QPushButton { "
        "  background-color: rgba(255, 255, 255, 30); "  // ✅ Полупрозрачный белый фон
        "  border: 1px solid rgba(255, 255, 255, 50); "  // ✅ Тонкая белая рамка
        "  border-radius: 3px; "
        "} "
        "QPushButton:hover { "
        "  background-color: rgba(255, 255, 255, 60); "  // ✅ Более яркий при наведении
        "} "
        "QPushButton:pressed { "
        "  background-color: rgba(255, 255, 255, 90); "
        "}"
        );
        soundButton->setToolTip("Переключить звук");
        connect(soundButton, &QPushButton::clicked, this, &CustomTitleBar::toggleSound);

        // ✅ ИНДИКАТОР
        indicator = new LedIndicator(this);
        indicator->setFixedSize(16, 16);
        indicator->setState(LedIndicator::State::Gray);  // ✅ Серый при загрузке (не горит)
        indicator->setStyleSheet("margin-right: 8px;");

        // Порядок элементов: текст → кнопка звука → индикатор
        layout->addWidget(titleLabel);
        layout->addWidget(soundButton);
        layout->addWidget(indicator);

 //       setStyleSheet("background-color: #005A9E; border: none; margin: 0px; padding: 0px 12px 0px 0px;"); // ✅ ДОБАВИТЬ padding-right
 //       setStyleSheet("background-color: #005A9E; border: none; margin: 0px; padding: 0px;");
 //       setStyleSheet("background-color: #005A9E; border: none; padding: 0px;");
        setMinimumHeight(28);
    }

    void setTitle(const QString &text) { titleLabel->setText(text); }
    LedIndicator* getIndicator() const { return indicator; }

    // ✅ Методы для управления звуком извне
    bool isSoundOn() const { return m_soundOn; }
    void setSoundOn(bool on) {
        if (m_soundOn != on) {
            m_soundOn = on;
            soundButton->setIcon(createSpeakerIcon(m_soundOn));
            emit soundStateChanged(m_soundOn);
        }
    }

    // ✅ НОВЫЙ МЕТОД: Установка цвета фона заголовка
    void setBackgroundColor(const QColor &color) {
        if (m_backgroundColor != color) {
            m_backgroundColor = color;
            update(); // Перерисовка
        }
    }

    protected:
    // ✅ ПЕРЕОПРЕДЕЛЁННЫЙ paintEvent для отрисовки фона
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.fillRect(rect(), m_backgroundColor);
        QWidget::paintEvent(event); // Рисуем дочерние виджеты поверх
    }

signals:
    // ✅ Сигнал, чтобы виджеты документа могли реагировать на переключение
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
    QColor m_backgroundColor = QColor("#005A9E"); // ✅ НОВОЕ: цвет фона заголовка

    // ✅ Рисует иконку динамика программно
    static QIcon createSpeakerIcon(bool enabled)
    {
        QPixmap pixmap(20, 20);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        // ✅ ИЗМЕНЕНО: используем белый цвет вместо чёрного
        QPen pen(Qt::white, 1.5);
        p.setPen(pen);
        p.setBrush(Qt::white);

        // Корпус динамика (трапеция)
        QPolygon speaker;
        speaker << QPoint(3, 8)
                << QPoint(7, 8)
                << QPoint(11, 4)
                << QPoint(11, 16)
                << QPoint(7, 12)
                << QPoint(3, 12);
        p.drawPolygon(speaker);

        if (enabled) {
            // Звуковые волны (две дуги)
            p.setBrush(Qt::NoBrush);
            p.drawArc(12, 6, 4, 8, -60 * 16, 120 * 16);  // Малая дуга
            p.drawArc(14, 3, 6, 14, -60 * 16, 120 * 16); // Большая дуга
        } else {
            // Перечёркивающий диагональный крест
            p.setPen(QPen(QColor(255, 100, 100), 2)); // Красноватая линия (остаётся)
            p.drawLine(13, 6, 19, 14);
            p.drawLine(19, 6, 13, 14);
        }
        p.end();
        return QIcon(pixmap);
    }
};


// ✅ Упрощенный класс подокна (без setTitleBarWidget)
class FixedSubWindow : public QMdiSubWindow {
public:
    explicit FixedSubWindow(QWidget *widget = nullptr, QWidget *parent = nullptr) : QMdiSubWindow(parent) {
        setWidget(widget);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        // ✅ Убираем стандартный заголовок ОС вместе с неработающим крестиком
        setWindowFlags(Qt::FramelessWindowHint | Qt::MSWindowsFixedSizeDialogHint);
    }
};


class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AppMode mode, const QJsonObject &clientConfig = QJsonObject(), QWidget *parent = nullptr);
    ~MainWindow();
    QList<QPair<QString, int>> getEquipmentListForRow(DocType type, int rowIndex);
    int getEquipmentRowCount(DocType type);
    QStringList getEquipmentConnectionNames(DocType type);  // ✅ НОВЫЙ МЕТОД
    // ✅ НОВЫЙ МЕТОД: Получить имя текущего авторизованного пользователя
    QString getCurrentUser() const { return authComboBox->currentText(); }
    // ✅ НОВЫЙ МЕТОД: Принудительная авторизация (вызывается из диалогов)
    void forceAuthenticate(const QString &username);

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
    // ✅ НОВЫЕ СЛОТЫ для авторизации
    void onAuthTimerTimeout();
    void onLogoutClicked();
    void showUserManagement(); // ✅ НОВЫЙ СЛОТ
    void showChangePassword(); // ✅ НОВЫЙ СЛОТ
    void showEquipmentSettings();      // ✅ НОВЫЙ СЛОТ
    void showEquipmentConfig();        // ✅ НОВЫЙ СЛОТ
    void showServerClientConfig();    // ✅ НОВЫЙ СЛОТ
    void updateWindowTitle();          // ✅ НОВЫЙ СЛОТ
    void showDatabaseConfig();    // ✅ НОВЫЙ СЛОТ

private:
    void setupActions();
    void applyUserPermissions();
    void loadConfig();
    void saveConfig();
    QString getAppDir() const;
    QString getConfigPath() const;
    void addDocumentPath(const QString &path);
    void removeDocumentPath(const QString &path);
    QString getActiveDocumentPath(); // ✅ НОВЫЙ МЕТОД
    void initializeDocumentFile(const QString &filePath, DocType type); // ✅ НОВЫЙ МЕТОД
    QString docTypeToString(DocType type); // ✅ НОВЫЙ МЕТОД
    QList<EquipmentConnection> getEquipmentConnections(const QString &filePath);
    void saveEquipmentConnections(const QString &filePath, const QList<EquipmentConnection> &connections);

    BaseDocWidget* createDocumentFromFile(const QString &path = QString(), DocType type = DocType::Antenna, bool addToConfig = true);
    void applyGridLayout();
    BaseDocWidget* findWidgetByPath(const QString &path) const;
    void openDocumentFromConfig(const QString &path, DocType type, quint16 docPort);
    void loadAppConfig();              // ✅ Загрузка конфига приложения

    // ✅ НОВЫЕ ПОЛЯ для управления панелями аварий
    QMap<QString, QWidget*> m_alarmPanels;  // docPath → AlarmPanel

    // ✅ НОВЫЕ МЕТОДЫ
    void updateAlarmPanelForDoc(const QString &docPath,
                                 const QList<int> &codes,
                                 const QList<int> &statuses);
    void removeAlarmPanelForDoc(const QString &docPath);

    // ✅ ДОБАВЛЕНО ЭТО ОБЪЯВЛЕНИЕ:
    void broadcastConfigToClients();

    QMdiArea *mdiArea;
    QMenu *fileMenu;
    QMenu *windowMenu;
    QToolBar *mainToolBar; // ⬅️ ДОБАВИТЬ
    QAction *saveAct;
    QAction *saveAsAct;
    QAction *closeAct;

    // ✅ НОВЫЕ действия для управления правами
    QAction *newAntAct;
    QAction *newTxAct;
    QAction *newModAct;
    QAction *newEnrAct;
    QAction *openAct;
    QAction *configAct;
    QAction *userConfigAct; // ✅ Для меню "Безопасность"
    QAction *changePasswordAct; // ✅ НОВОЕ ДЕЙСТВИЕ
    QAction *equipmentSettingsAct;     // ✅ НОВОЕ ДЕЙСТВИЕ
    QAction *equipmentConfigAct;       // ✅ НОВОЕ ДЕЙСТВИЕ
    QAction *serverClientConfigAct;   // ✅ НОВОЕ ДЕЙСТВИЕ
    QString m_programName;            // ✅ Название программы из конфига
    QAction *databaseConfigAct;   // ✅ НОВОЕ ДЕЙСТВИЕ

    // ✅ Структура для хранения параметров БД
    struct DatabaseConfig {
        QString host = "localhost";
        int port = 5432;
        QString dbName = "rmo_db";
        QString user = "postgres";
        QString password;
    };
    DatabaseConfig m_dbConfig;    // ✅ Параметры БД из конфига
    void loadDatabaseConfig();    // ✅ Загрузка параметров БД

    // ✅ ЭЛЕМЕНТЫ ПАНЕЛИ АВТОРИЗАЦИИ
    QLabel *authLabel;
    QComboBox *authComboBox;
    QLabel *timeLabel;
    QToolButton *logoutButton;
    QTimer *authTimer;
    int authTimeSeconds; // Хранит оставшееся время в секундах

    ScreenConfig config;
    NetworkController *m_net;
    AppMode m_mode;
};

#endif // MAINWINDOW_H
