#ifndef BORDEREDCUSTOMPLOT_H
#define BORDEREDCUSTOMPLOT_H

#include "qcustomplot.h"

// ============================================================================
// Подкласс QCustomPlot, который после отрисовки графика
// рисует вокруг виджета чёрную рамку 1 px.
//
// Причина: QCustomPlot ставит WA_OpaquePaintEvent и полностью
// перекрывает всё, что рисуется снаружи его paintEvent.
// Поэтому рамку нужно рисовать изнутри — в переопределённом paintEvent.
// ============================================================================
class BorderedCustomPlot : public QCustomPlot
{
    Q_OBJECT
public:
    using QCustomPlot::QCustomPlot;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        // 1. Сначала штатная отрисовка QCustomPlot:
        //    фон, сетка, оси, кривые, метки, легенда.
        QCustomPlot::paintEvent(event);

        // 2. Поверх всего рисуем рамку в том же проходе.
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(QPen(Qt::black, 1));
        painter.setBrush(Qt::NoBrush);
        // adjusted(0, 0, -1, -1) — правый и нижний край
        // остаются внутри виджета, иначе 1 px уедет за границу.
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
        painter.end();
    }
};

#endif // BORDEREDCUSTOMPLOT_H
