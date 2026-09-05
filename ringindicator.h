#ifndef RINGINDICATOR_H
#define RINGINDICATOR_H

#include <QWidget>
#include <QColor>
#include <QTimer>

class RingIndicator : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY stateChanged)

public:
    enum class State {
        Gray,
        Green,
        Yellow,
        Red
    };

    explicit RingIndicator(QWidget *parent = nullptr);

    void setState(State state);
    State state() const { return m_state; }

    // --- Управление индикацией ---
    void setBlinking(bool blink);
    bool isBlinking() const { return m_blinking; }

    // Метод "триггера" (вспышка на заданное время)
    void trigger(int durationMs = 1000);

    // *** МЕТОДЫ ДЛЯ Q_PROPERTY ***
    bool isActive() const;
    void setActive(bool active);

    // ✅ НОВОЕ: Настройка линий-стрелок
    void setLineThickness(int thickness);
    int lineThickness() const { return m_lineThickness; }

    void setLineColor(const QColor &color);
    QColor lineColor() const { return m_lineColor; }

    // ✅ НОВОЕ: Автоматический цвет линий по состоянию (вкл/выкл)
    void setAutoLineColor(bool enabled);
    bool isAutoLineColor() const { return m_autoLineColor; }

signals:
    void stateChanged(bool active);
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onBlinkTimeout();
    void onTriggerTimeout();

private:
    State m_state = State::Gray;
    int m_thickness = 3;

    // ✅ НОВОЕ: Параметры линий-стрелок
    int m_lineThickness = 4;          // Толщина линий
    QColor m_lineColor = QColor(100, 100, 100);  // Цвет линий (по умолчанию серый)
    bool m_autoLineColor = true;      // Автоматически менять цвет по состоянию

    // Флаги состояния отображения
    bool m_isLit = true;
    bool m_blinking = false;
    bool m_triggerActive = false;

    QTimer *m_blinkTimer;
    QTimer *m_triggerTimer;

    // ✅ НОВОЕ: Вспомогательный метод для получения цвета линии
    QColor getEffectiveLineColor() const;
};

#endif // RINGINDICATOR_H
