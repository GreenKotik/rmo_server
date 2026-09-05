#ifndef ALARMPANEL_H
#define ALARMPANEL_H

#include <QWidget>
#include <QVector>
#include <QDateTime>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QListView>

// Структура AlarmEntry должна быть доступна везде
struct AlarmEntry {
    int code = 0;
    int status = 0; // 0: Info, 1: Critical, 2: Command
    QString message;
    QDateTime timestamp;
    int deviceId = 0;
};

// *** МОДЕЛЬ ДАННЫХ ***
class AlarmListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { CodeRole = Qt::UserRole + 1, StatusRole, MessageRole, TimeRole };

    explicit AlarmListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setAlarms(const QList<AlarmEntry> &alarms);
    int criticalAlarmCount() const;

private:
    QVector<AlarmEntry> m_alarms;
};

// *** ДЕЛЕГАТ ОТДИЗАЙНА ***
class AlarmItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit AlarmItemDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

//private:
    static constexpr int ROW_HEIGHT = 24;
};

// *** ОСНОВНОЙ ВИДЖЕТ ПАНЕЛИ ***
class AlarmPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmPanel(QWidget *parent = nullptr);
    ~AlarmPanel();

    void setAlarms(const QList<int> &codes, const QList<int> &statuses);
    void clearAlarms();
    int criticalAlarmCount() const;
    int calculateHeight() const;

signals:
    void alarmCountChanged(int count);

private:
    // UI элементы нового типа
    QListView *m_alarmView = nullptr;

    // Data Layer
    AlarmListModel *m_model = nullptr;
    AlarmItemDelegate *m_delegate = nullptr;

    static constexpr int MAX_HEIGHT = 300;

protected:
    void paintEvent(QPaintEvent *event) override;;
};


#endif // ALARMPANEL_H
