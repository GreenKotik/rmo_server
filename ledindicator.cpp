#include "ledindicator.h"
#include <QPainter>

LedIndicator::LedIndicator(QWidget *parent)
    : QWidget(parent),
      m_state(State::Gray),
      m_blinkTimer(new QTimer(this)),
      m_triggerTimer(new QTimer(this)),
      m_blinking(false),
      m_blinkInterval(500),
      m_isLit(true),
      m_triggerActive(false)
{
    m_blinkTimer->setSingleShot(false);
    connect(m_blinkTimer, &QTimer::timeout, this, &LedIndicator::onBlinkTimeout);

    // ✅ Настройка таймера триггера (однократный запуск)
    m_triggerTimer->setSingleShot(true);
    connect(m_triggerTimer, &QTimer::timeout, this, &LedIndicator::onTriggerTimeout);

    setColor(Qt::gray);
}

void LedIndicator::setState(State state)
{
    m_state = state;
    switch (state) {
        case State::Green:  setColor(QColor(0, 200, 0)); break;
        case State::Red:    setColor(QColor(220, 50, 50)); break;
        case State::Yellow: setColor(QColor(255, 200, 0)); break;
        case State::Gray:   setColor(QColor(150, 150, 150)); break;
    }
}

void LedIndicator::setColor(const QColor &color)
{
    if (m_color != color) {
        m_color = color;
        if (!m_triggerActive) {
            m_isLit = true;
        }
        emit colorChanged(m_color);
        update();
    }
}

void LedIndicator::setBlinking(bool blink)
{
    if (m_blinking != blink) {
        m_blinking = blink;
        if (m_blinking) {
            m_triggerTimer->stop(); // Триггер отменяет мигание
            m_triggerActive = false;
            m_isLit = true;
            m_blinkTimer->start(m_blinkInterval);
        } else {
            m_blinkTimer->stop();
            m_isLit = true;
            update();
        }
        emit blinkingChanged(m_blinking);
    }
}

void LedIndicator::setBlinkInterval(int ms)
{
    if (m_blinkInterval != ms && ms > 0) {
        m_blinkInterval = ms;
        if (m_blinking) {
            m_blinkTimer->start(m_blinkInterval);
        }
        emit blinkIntervalChanged(m_blinkInterval);
    }
}

// ✅ РЕАЛИЗАЦИЯ ТРИГГЕРА
void LedIndicator::trigger(int durationMs)
{
    if (durationMs <= 0) return;

    // Временно останавливаем мигание, чтобы триггер имел приоритет
    m_blinkTimer->stop();
    m_triggerActive = true;
    m_isLit = true; // Принудительно зажигаем

    update(); // Мгновенная перерисовка
    m_triggerTimer->start(durationMs); // Запуск таймера затухания
}

void LedIndicator::onBlinkTimeout()
{
    if (!m_triggerActive) {
        m_isLit = !m_isLit;
        update();
    }
}

// ✅ ОБРАБОТЧИК ОКОНЧАНИЯ ТРИГГЕРА ("затухание")
void LedIndicator::onTriggerTimeout()
{
    m_triggerActive = false;
    m_isLit = false; // "Гасим" индикатор
    update();

    // Примечание: Если нужно, чтобы после триггера индикатор
    // автоматически возвращался к миганию, раскомментируйте строку ниже:
    // if (m_blinking) m_blinkTimer->start(m_blinkInterval);
}

void LedIndicator::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor drawColor = m_color;

    // ✅ Логика "затухания": если не горит, делаем цвет очень темным
    // (имитация выключенной светодиодной лампочки)
    if (!m_isLit) {
        drawColor = m_color.darker(300);
        // Альтернатива: drawColor = Qt::transparent; (полная прозрачность)
    }

    QRadialGradient gradient(rect().center(), rect().width() / 2.0);

    if (!m_isLit) {
        // Для выключенного состояния градиент более плоский и темный
        gradient.setColorAt(0.0, drawColor.lighter(110));
        gradient.setColorAt(1.0, drawColor.darker(120));
        painter.setPen(QPen(drawColor.darker(150), 1));
    } else {
        // Для включенного состояния яркий 3D-эффект с бликом
        gradient.setColorAt(0.0, drawColor.lighter(150));
        gradient.setColorAt(0.4, drawColor);
        gradient.setColorAt(1.0, drawColor.darker(150));
        painter.setPen(QPen(drawColor.darker(200), 1));
    }

    painter.setBrush(gradient);
    painter.drawEllipse(rect().adjusted(1, 1, -1, -1));
}
