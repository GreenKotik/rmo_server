#include "ringindicator.h"
#include <QPainter>
#include <QDebug>
#include <cmath>

RingIndicator::RingIndicator(QWidget *parent)
    : QWidget(parent),
      m_blinkTimer(new QTimer(this)),
      m_triggerTimer(new QTimer(this))
{
    setFixedSize(150, 150);

    connect(m_blinkTimer, &QTimer::timeout, this, &RingIndicator::onBlinkTimeout);
    m_triggerTimer->setSingleShot(true);
    connect(m_triggerTimer, &QTimer::timeout, this, &RingIndicator::onTriggerTimeout);
}

// Реализация методов-геттеров/сеттеров для Q_PROPERTY
bool RingIndicator::isActive() const
{
    return (m_state != State::Gray);
}

void RingIndicator::setActive(bool active)
{
    if (active) {
        if (m_state == State::Gray) {
            setState(State::Green);
        }
    } else {
        setState(State::Gray);
    }
}

void RingIndicator::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        if (state != State::Gray && !m_isLit && !m_triggerActive) {
            m_isLit = true;
        }
        update();
        emit stateChanged(isActive());
    }
}

void RingIndicator::setBlinking(bool blink)
{
    if (m_blinking != blink) {
        m_blinking = blink;
        if (m_blinking) {
            m_triggerTimer->stop();
            m_triggerActive = false;
            m_isLit = true;
            m_blinkTimer->start(500);
        } else {
            m_blinkTimer->stop();
            m_isLit = (m_state != State::Gray);
        }
        update();
    }
}

void RingIndicator::trigger(int durationMs)
{
    if (durationMs <= 0) return;

    m_blinkTimer->stop();
    m_triggerActive = true;
    m_isLit = true;
    update();
    m_triggerTimer->start(durationMs);
}

void RingIndicator::onBlinkTimeout()
{
    if (!m_triggerActive) {
        m_isLit = !m_isLit;
        update();
    }
}

void RingIndicator::onTriggerTimeout()
{
    m_triggerActive = false;
    m_isLit = (m_state != State::Gray);
    update();
}

// ✅ НОВОЕ: Методы управления линиями
void RingIndicator::setLineThickness(int thickness)
{
    if (m_lineThickness != thickness && thickness > 0) {
        m_lineThickness = thickness;
        update();
    }
}

void RingIndicator::setLineColor(const QColor &color)
{
    if (m_lineColor != color) {
        m_lineColor = color;
        m_autoLineColor = false;  // Ручная установка отключает авто-режим
        update();
    }
}

void RingIndicator::setAutoLineColor(bool enabled)
{
    if (m_autoLineColor != enabled) {
        m_autoLineColor = enabled;
        update();
    }
}

// ✅ НОВОЕ: Получение эффективного цвета линии
QColor RingIndicator::getEffectiveLineColor() const
{
    if (!m_autoLineColor) {
        // Ручной режим — используем заданный цвет
        QColor c = m_lineColor;
        if (!m_isLit) c.setAlphaF(0.3);
        return c;
    }

    // Автоматический режим — цвет зависит от состояния
    QColor color;
    switch (m_state) {
        case State::Green:  color = QColor(76, 175, 80); break;    // Зелёный
        case State::Yellow: color = QColor(255, 200, 0); break;    // Янтарный
        case State::Red:    color = QColor(244, 67, 54); break;    // Красный
        case State::Gray:
        default:            color = QColor(150, 150, 150); break;  // Серый
    }

    if (!m_isLit) {
        color.setAlphaF(0.3);  // Полупрозрачный, если "выключен"
    }
    return color;
}

// ============================================================================
// ✅ ОТРИСОВКА ВИДЖЕТА
// ============================================================================
void RingIndicator::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Фон всей панели
    painter.fillRect(rect(), QColor("#F5F5F5"));

    // --- Логика цвета кольца ---
    QColor ringColor;
    switch (m_state) {
        case State::Green:  ringColor = QColor(76, 175, 80); break;
        case State::Yellow: ringColor = QColor(255, 200, 0); break;
        case State::Red:    ringColor = QColor(244, 67, 54); break;
        case State::Gray:
        default:            ringColor = QColor(150, 150, 150); break;
    }

    if (!m_isLit) {
        ringColor.setAlphaF(0.3);
    }

    // --- Параметры кольца ---
    const int penWidth = 2;
    QPen pen(ringColor);
    pen.setWidth(penWidth);
    pen.setCapStyle(Qt::FlatCap);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(pen);

    int centerX = width() / 2;
    int centerY = height() / 2;
    const int RING_SIZE = 120;
    const double R = RING_SIZE / 2.0;

    QRectF drawArea(
        centerX - R,
        centerY - R,
        RING_SIZE,
        RING_SIZE
    );
    painter.drawEllipse(drawArea.adjusted(penWidth/2.0, penWidth/2.0, -penWidth/2.0, -penWidth/2.0));

    // =========================================================================
    // ✅ ОТРИСОВКА ТРЁХ ЛИНИЙ-СТРЕЛОК ПОД УГЛОМ 60°
    // =========================================================================

    // Получаем эффективный цвет линий
    QColor lineColor;
    switch (m_state) {
        case State::Green:  lineColor = QColor(76, 175, 80); break;
        case State::Yellow: lineColor = QColor(255, 200, 0); break;
        case State::Red:    lineColor = QColor(244, 67, 54); break;
        case State::Gray:
        default:            lineColor = QColor(150, 150, 150); break;
    }
    if (!m_isLit) {
        lineColor.setAlphaF(0.3);
    }

    QPen linePen(lineColor);
    linePen.setWidth(m_lineThickness);
    linePen.setCapStyle(Qt::RoundCap);
    linePen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(linePen);

    // ✅ Тригонометрия для угла 60°
    // cos(60°) = 0.5, sin(60°) ≈ 0.8660
    const double cos60 = 0.5;
    const double sin60 = 0.8660;

    // Точки пересечения с кольцом
    // Линия 1: влево-вверх (угол 60° от горизонтали)
    QPointF ringTopLeft(centerX - R * cos60, centerY - R * sin60);
    // Линия 2: влево-вниз (угол 60° от горизонтали)
    QPointF ringBottomLeft(centerX - R * cos60, centerY + R * sin60);
    QPointF center(centerX, centerY);

    // =========================================================================
    // ✅ ЛИНИЯ 1: Центр → 60° влево-вверх до кольца → горизонтально влево до края
    // =========================================================================
    QPolygonF line1;
    line1 << center
          << ringTopLeft
          << QPointF(0, ringTopLeft.y());
    painter.drawPolyline(line1);

    // =========================================================================
    // ✅ ЛИНИЯ 2: Центр → 60° влево-вниз до кольца → горизонтально влево до края
    // =========================================================================
    QPolygonF line2;
    line2 << center
          << ringBottomLeft
          << QPointF(0, ringBottomLeft.y());
    painter.drawPolyline(line2);

    // =========================================================================
    // ✅ ЛИНИЯ 3: Центр → горизонтально вправо до края
    // =========================================================================
    painter.drawLine(center, QPointF(width(), centerY));
}
