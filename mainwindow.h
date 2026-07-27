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

struct ScreenConfig {
    int columns = 2;
    QStringList docPaths;
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
public:
    explicit FixedSubWindow(QWidget *widget = nullptr, QWidget *parent = nullptr) : QMdiSubWindow(parent) {
        setWidget(widget);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
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
    QStringList getEquipmentConnectionNames(DocType type);
    QString getCurrentUser() const { return authComboBox->currentText(); }
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
    void onAuthTimerTimeout();
    void onLogoutClicked();
    void showUserManagement();
    void showChangePassword();
    void showEquipmentSettings();
    void showEquipmentConfig();
    void showServerClientConfig();
    void updateWindowTitle();
    void showDatabaseConfig();

private:
    void setupActions();
    void applyUserPermissions();
    void loadConfig();
    void saveConfig();
    QString getAppDir() const;
    QString getConfigPath() const;
    void addDocumentPath(const QString &path);
    void removeDocumentPath(const QString &path);
    QString getActiveDocumentPath();
    void initializeDocumentFile(const QString &filePath, DocType type);
    QString docTypeToString(DocType type);
    QList<EquipmentConnection> getEquipmentConnections(const QString &filePath);
    void saveEquipmentConnections(const QString &filePath, const QList<EquipmentConnection> &connections);
    BaseDocWidget* createDocumentFromFile(const QString &path = QString(), DocType type = DocType::Antenna, bool addToConfig = true);
    void applyGridLayout();
    BaseDocWidget* findWidgetByPath(const QString &path) const;
    void openDocumentFromConfig(const QString &path, DocType type, quint16 docPort);
    void loadAppConfig();
    void updateAlarmPanelForDoc(const QString &docPath, const QList<int> &codes, const QList<int> &statuses);
    void removeAlarmPanelForDoc(const QString &docPath);
    void broadcastConfigToClients();

    QPointer<QMdiArea> mdiArea;
    QMenu *fileMenu;
    QMenu *windowMenu;
    QToolBar *mainToolBar;
    QAction *saveAct;
    QAction *saveAsAct;
    QAction *closeAct;
    QAction *newAntAct;
    QAction *newTxAct;
    QAction *newModAct;
    QAction *newEnrAct;
    QAction *openAct;
    QAction *configAct;
    QAction *userConfigAct;
    QAction *changePasswordAct;
    QAction *equipmentSettingsAct;
    QAction *equipmentConfigAct;
    QAction *serverClientConfigAct;
    QString m_programName;
    QAction *databaseConfigAct;

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
};

#endif // MAINWINDOW_H
