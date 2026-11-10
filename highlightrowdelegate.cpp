#include "highlightrowdelegate.h"
#include "measurementsmodel.h"
#include "dataviewproxies.h"
#include <QPainter>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QApplication>
#include <QStyle>

HighlightRowDelegate::HighlightRowDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize HighlightRowDelegate::sizeHint(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    QSize base = QStyledItemDelegate::sizeHint(option, index);
    if (!index.isValid() || !m_proxy || !m_model) return base;

    const QModelIndex srcIdx = m_proxy->mapToSource(index);
    if (!srcIdx.isValid()) return base;

    // Считаем высоту только для столбца "Аварии"
    if (srcIdx.column() != m_model->errorColumn()) {
        return base;
    }

    const int errorCount = m_model->rowErrorCountCached(srcIdx.row());
    const int lines = qMax(1, errorCount);

    const int textHeight = option.fontMetrics.height() + m_linePadding;

    int h = textHeight * lines + m_verticalPadding * 2;
    if (h > m_maxHeight) h = m_maxHeight;

    return QSize(base.width(), h);
}

void HighlightRowDelegate::paint(QPainter *painter,
                                 const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;

    // ✅ Особый случай: столбец "ErrorsAll" в tableData — рисуем расшифровку
    if (m_proxy && m_model) {
        const QModelIndex srcIdx = m_proxy->mapToSource(index);
        if (srcIdx.isValid() && srcIdx.column() == m_model->errorColumn()) {
            // Фон
            QStyleOptionViewItem opt = option;
            QStyle *style = option.widget ? option.widget->style() : QApplication::style();
            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, option.widget);

            // Жёлтая подсветка текущей строки (если это она)
            const QAbstractItemView *view = qobject_cast<const QAbstractItemView*>(parent());
            bool isCurrentRow = false;
            if (view && view->selectionModel()) {
                const QModelIndex current = view->selectionModel()->currentIndex();
                if (current.isValid() && current.row() == index.row())
                    isCurrentRow = true;
            }
            if (isCurrentRow) {
                painter->fillRect(option.rect, m_highlightColor);
            }

            const QStringList messages = m_model->rowErrorMessages(srcIdx.row());
            if (messages.isEmpty()) return;

            painter->save();
            painter->setPen(QColor(198, 40, 40));
            const int textHeight = option.fontMetrics.height() + m_linePadding;
            const int x = option.rect.left() + 6;
            const int y0 = option.rect.top() + m_verticalPadding;
            const int width = option.rect.width() - 12;

            for (int i = 0; i < messages.size(); ++i) {
                const int y = y0 + i * textHeight;
                if (y + textHeight > option.rect.bottom() + 1) break;
                painter->drawText(QRect(x, y, width, textHeight),
                                  Qt::AlignLeft | Qt::AlignVCenter,
                                  messages.at(i));
            }
            painter->restore();
            return;
        }
    }

    // Проверяем, текущая ли это строка
    bool isCurrentRow = false;
    const QAbstractItemView *view = qobject_cast<const QAbstractItemView*>(parent());
    if (view && view->selectionModel()) {
        const QModelIndex current = view->selectionModel()->currentIndex();
        if (current.isValid() && current.row() == index.row())
            isCurrentRow = true;
    }

    if (isCurrentRow) {
        painter->save();
        painter->fillRect(opt.rect, m_highlightColor);
        painter->restore();

        opt.state &= ~QStyle::State_Selected;
        opt.state &= ~QStyle::State_MouseOver;
        opt.palette.setColor(QPalette::Text, Qt::black);
        opt.palette.setColor(QPalette::HighlightedText, Qt::black);
    } else if (!m_specialColumnName.isEmpty()
               && index.data(Qt::DisplayRole).isValid())
    {
        // ✅ Сравниваем по имени столбца, а не по индексу
        // Заголовок столбца одинаков в источнике и в прокси,
        // потому что прокси не переименовывает столбцы.
        // Получаем имя столбца через headerData.
        const QString colName = index.model()->headerData(
            index.column(), Qt::Horizontal, Qt::DisplayRole).toString();

        if (colName == m_specialColumnName) {
            painter->save();
            painter->fillRect(opt.rect, m_specialColor);
            painter->restore();
        }
    }

    QStyledItemDelegate::paint(painter, opt, index);
}


// ============================================================================
// JournalHeightDelegate::sizeHint
// ============================================================================
QSize JournalHeightDelegate::sizeHint(const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const
{
    QSize base = QStyledItemDelegate::sizeHint(option, index);
    if (!index.isValid() || !m_proxy || !m_model) return base;

    const QModelIndex srcIdx = m_proxy->mapToSource(index);
    if (!srcIdx.isValid()) return base;

    // Высота имеет смысл только для столбца "Аварии"
    if (srcIdx.column() != m_model->errorColumn()) {
        return base;
    }

    const int errorCount = m_model->rowErrorCountCached(srcIdx.row());
    const int lines = qMax(1, errorCount);

    // Одна строка = высота шрифта + межстрочный зазор
    const int textHeight = option.fontMetrics.height() + m_linePadding;

    int h = textHeight * lines + m_verticalPadding * 2;
    if (h > m_maxHeight) h = m_maxHeight;

    return QSize(base.width(), h);
}


void JournalHeightDelegate::paint(QPainter *painter,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    if (!index.isValid() || !m_proxy || !m_model) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    const QModelIndex srcIdx = m_proxy->mapToSource(index);
    if (!srcIdx.isValid()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // ✅ Проверяем, является ли строка текущей (выделенной)
    bool isCurrentRow = false;
    const QAbstractItemView *view = qobject_cast<const QAbstractItemView*>(parent());
    if (view && view->selectionModel()) {
        const QModelIndex current = view->selectionModel()->currentIndex();
        if (current.isValid() && current.row() == index.row()) {
            isCurrentRow = true;
        }
    }

    // ✅ Светло-жёлтый цвет для подсветки текущей строки
    const QColor highlightColor = HighlightRowDelegate::defaultHighlightColor();

    const int errorsCol = m_model->errorColumn();
    const bool isErrorsCol = (errorsCol >= 0 && srcIdx.column() == errorsCol);

    const QString colName = m_model->headerData(
        srcIdx.column(), Qt::Horizontal, Qt::DisplayRole).toString();
    const bool isSpecialCol = (!m_specialColumnName.isEmpty() &&
                               colName == m_specialColumnName);

    // --- Случай 1: столбец "Аварии" — многострочный текст ---
    if (isErrorsCol) {
        const QStringList messages = m_model->rowErrorMessages(srcIdx.row());
        if (messages.isEmpty()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        painter->save();

        // 1. Фон ячейки (стандартный: чередование, выделение, etc.)
        QStyleOptionViewItem opt = option;
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, option.widget);

        // 2. ✅ Жёлтая подсветка текущей строки (поверх стандартного фона)
        if (isCurrentRow) {
            painter->fillRect(option.rect, highlightColor);
        }

        // 3. Текст аварий (красный)
        painter->setPen(QColor(198, 40, 40));
        const int textHeight = option.fontMetrics.height() + m_linePadding;
        const int x = option.rect.left() + m_horizontalPadding;
        const int y0 = option.rect.top() + m_verticalPadding;
        const int width = option.rect.width() - m_horizontalPadding * 2;

        for (int i = 0; i < messages.size(); ++i) {
            const int y = y0 + i * textHeight;
            if (y + textHeight > option.rect.bottom() + 1) break;
            painter->drawText(QRect(x, y, width, textHeight),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              messages.at(i));
        }

        painter->restore();
        return;
    }

    // --- Случай 2: столбец "Дата и время" — фон ---
    if (isSpecialCol) {
        QStyleOptionViewItem opt = option;

        // 1. Стандартный фон (для выделения/чередования)
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, option.widget);

        // 2. ✅ Если строка текущая — жёлтый (приоритетнее голубого)
        //    Иначе — голубовато-серый специальный цвет
        painter->save();
        if (isCurrentRow) {
            painter->fillRect(opt.rect, highlightColor);
        } else {
            painter->fillRect(opt.rect, m_specialColor);
        }
        painter->restore();

        // Отключаем Selected/MouseOver в стиле, чтобы текст не стал белым
        opt.state &= ~QStyle::State_Selected;
        opt.state &= ~QStyle::State_MouseOver;
        opt.palette.setColor(QPalette::Text, Qt::black);
        opt.palette.setColor(QPalette::HighlightedText, Qt::black);

        QStyledItemDelegate::paint(painter, opt, index);
        return;
    }

    // --- Случай 3: остальные столбцы — стандартная отрисовка ---
    // ✅ Если строка текущая — заливаем жёлтым
    if (isCurrentRow) {
        QStyleOptionViewItem opt = option;
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, option.widget);

        painter->save();
        painter->fillRect(opt.rect, highlightColor);
        painter->restore();

        opt.state &= ~QStyle::State_Selected;
        opt.state &= ~QStyle::State_MouseOver;
        opt.palette.setColor(QPalette::Text, Qt::black);
        opt.palette.setColor(QPalette::HighlightedText, Qt::black);

        QStyledItemDelegate::paint(painter, opt, index);
        return;
    }

    QStyledItemDelegate::paint(painter, option, index);
}
