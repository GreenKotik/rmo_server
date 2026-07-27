#ifndef LEDINDICATOR_H
#define LEDINDICATOR_H

#include <QWidget>
#include <QColor>
#include <QTimer>

class LedIndicator : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(bool blinking READ isBlinking WRITE setBlinking NOTIFY blinkingChanged)
    Q_PROPERTY(int blinkInterval READ blinkInterval WRITE setBlinkInterval NOTIFY blinkIntervalChanged)

public:
    enum class State {
        Green,
        Red,
        Yellow,
        Gray
    };

    explicit LedIndicator(QWidget *parent = nullptr);

    void setState(State state);
    State state() const { return m_state; }

    void setColor(const QColor &color);
    QColor color() const { return m_color; }

    bool isBlinking() const { return m_blinking; }
    void setBlinking(bool blink);

    int blinkInterval() const { return m_blinkInterval; }
    void setBlinkInterval(int ms);

    // ✅ НОВЫЙ МЕТОД: Одиночный триггер (импульс)
    // Загорается текущим цветом и гаснет через durationMs миллисекунд
    void trigger(int durationMs = 1000);

    QSize sizeHint() const override { return QSize(24, 24); }

signals:
    void colorChanged(const QColor &color);
    void blinkingChanged(bool);
    void blinkIntervalChanged(int);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onBlinkTimeout();
    void onTriggerTimeout(); // ✅ Обработчик окончания триггера

private:
    State m_state;
    QColor m_color;

    QTimer *m_blinkTimer;
    QTimer *m_triggerTimer; // ✅ Отдельный таймер для триггера

    bool m_blinking;
    int m_blinkInterval;

    bool m_isLit;       // Флаг: горит ли индикатор в данный момент
    bool m_triggerActive; // Флаг: активен ли сейчас режим триггера
};

#endif // LEDINDICATOR_H
