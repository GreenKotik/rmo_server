#ifndef ALARMPANEL_H
#define ALARMPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QDateTime>
#include <QList>
#include <QColor>

// ============================================================================
// ✅ СТРУКТУРА ДЛЯ ХРАНЕНИЯ ОДНОЙ АВАРИИ
// ============================================================================
struct AlarmEntry {
    int code;
    int status;
    QString message;
    QDateTime timestamp;
    int deviceId;
};

// ============================================================================
// ✅ ВИДЖЕТ ПАНЕЛИ АВАРИЙ (визуальное продолжение окна документа)
// ============================================================================
class AlarmPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmPanel(QWidget *parent = nullptr);

    // ✅ Полное обновление списка аварий
    void setAlarms(const QList<int> &codes, const QList<int> &statuses);

    // ✅ Очистить все аварии
    void clearAlarms();

    // ✅ Получить количество активных аварий (Status=1)
    int criticalAlarmCount() const;

    // ✅ Рассчитать высоту панели на основе количества аварий
    int calculateHeight() const;

    // ✅ Константа: высота одной строки аварии
    static constexpr int ROW_HEIGHT = 24;

    // ✅ Максимальная высота панели (чтобы не сломать интерфейс)
    static constexpr int MAX_HEIGHT = 300;

signals:
    void alarmCountChanged(int count);

private:
    void updateDisplay();
    void rebuildAlarmList();
    QColor getAlarmColor(int status) const;

    QVBoxLayout *m_mainLayout;
    QWidget *m_alarmContainer;
    QVBoxLayout *m_alarmLayout;

    QList<AlarmEntry> m_alarms;
};

#endif // ALARMPANEL_H
