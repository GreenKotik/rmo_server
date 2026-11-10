#include "alarmpanel.h"
#include "alarmregistry.h"
#include <QDebug>
#include <QPainter>
#include <algorithm>
#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent>

namespace AlarmColors {
    static const QColor CRITICAL_BG(255, 235, 238);
    static const QColor INFO_BG(255, 249, 196);
    static const QColor COMMAND_BG(227, 242, 253);
    static const QColor TEXT_NORMAL(66, 66, 66);
    static const QColor TEXT_CRITICAL(198, 40, 40);
    static const QColor TEXT_INFO(245, 127, 23);
}

// Реализация Модели (без изменений)
AlarmListModel::AlarmListModel(QObject *parent) : QAbstractListModel(parent) {}
int AlarmListModel::rowCount(const QModelIndex & /*parent*/) const { return m_alarms.size(); }

QVariant AlarmListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_alarms.size()) return QVariant();
    const auto &entry = m_alarms.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case MessageRole: return entry.message;
    case CodeRole: return entry.code;
    case StatusRole: return entry.status;
    case TimeRole: return entry.timestamp.toString("HH:mm:ss");
    default: return QVariant();
    }
}

QHash<int, QByteArray> AlarmListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[MessageRole] = "message";
    roles[CodeRole] = "code";
    roles[StatusRole] = "status";
    roles[TimeRole] = "time";
    return roles;
}

void AlarmListModel::setAlarms(const QList<AlarmEntry> &alarms) {
    beginResetModel();
    m_alarms.clear();
    for (const auto &raw : alarms) {
        AlarmInfo info = AlarmRegistry::instance().getInfo(raw.code);
        AlarmEntry e{raw.code, raw.status, info.mesRus, raw.timestamp, raw.deviceId};
        m_alarms.append(e);
    }
    std::stable_sort(m_alarms.begin(), m_alarms.end(), [](const AlarmEntry &a, const AlarmEntry &b){
        if (a.status != b.status) return a.status > b.status;
        return a.timestamp > b.timestamp;
    });
    endResetModel();
}

int AlarmListModel::criticalAlarmCount() const {
    int count = 0;
    for (const auto &e : m_alarms) if (e.status == 1) count++;
    return count;
}

bool AlarmListModel::removeAlarmByCode(int code)
{
    for (int row = 0; row < m_alarms.size(); ++row) {
        if (m_alarms[row].code == code) {
            beginRemoveRows(QModelIndex(), row, row);
            m_alarms.removeAt(row);
            endRemoveRows();
            return true;
        }
    }
    return false;
}

// Реализация Делегата
AlarmItemDelegate::AlarmItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void AlarmItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    painter->save();

    // --- 1. БЫСТРЫЙ ФОН  ---
    painter->setRenderHint(QPainter::Antialiasing, false); // Оставляем выключенным для прямоугольников

    int status = index.data(AlarmListModel::StatusRole).toInt();
    QColor bg = (status == 1) ? AlarmColors::CRITICAL_BG :
                (status == 0) ? AlarmColors::INFO_BG :
                                AlarmColors::COMMAND_BG;

    // Рисуем фон ячейки
    painter->fillRect(option.rect, bg);

    // --- 2. ТЕКСТ (со сглаживанием и жирностью) ---
    QString time = index.data(AlarmListModel::TimeRole).toString();
    QString codeStr = QString("[%1]").arg(index.data(AlarmListModel::CodeRole).toInt());
    QString text = index.data(AlarmListModel::MessageRole).toString();

    QColor penColor = (status == 1) ? AlarmColors::TEXT_CRITICAL :
                      (status == 0) ? AlarmColors::TEXT_INFO :
                                      AlarmColors::TEXT_NORMAL;

    // Берем шрифт из настроек системы/приложения
    QFont font = option.font;

    // Делаем текст жирным (Bold)
    font.setWeight(QFont::Bold); // Или QFont::DemiBold (чуть тоньше)

    // Включаем сглаживание ИМЕННО ДЛЯ ТЕКСТА
    // Подсказываем рендереру использовать технологии ClearType/Smooth для букв
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setRenderHint(QPainter::Antialiasing, true);     // Для четких краев глифов

    painter->setFont(font);
    painter->setPen(penColor);

    const int marginLeft = 8;
    const int spacing = 10;
    QRect r = option.rect.adjusted(marginLeft, 0, -marginLeft, 0);
    int xPos = r.left();

    // Высота текста должна соответствовать высоте строки ROW_HEIGHT
    int textHeight = AlarmItemDelegate::ROW_HEIGHT;

    painter->drawText(xPos, r.top(), 64, textHeight, Qt::AlignVCenter, time);
    xPos += 64 + spacing;

    painter->setPen(Qt::gray);
    painter->drawText(xPos, r.top(), 50, textHeight, Qt::AlignVCenter | Qt::AlignHCenter, codeStr);
    xPos += 50 + spacing;

    painter->setPen(penColor);
    painter->drawText(xPos, r.top(), r.width() - xPos, textHeight, Qt::AlignVCenter, text);

    // Рисуем кнопку «Подтвердить» справа.
    const int btnH = qMin(option.rect.height() - 6, 18);
    const int btnW = ACK_BTN_WIDTH - 8;
    const QRect btnRect(
        option.rect.right() - ACK_BTN_WIDTH + 4,
        option.rect.top() + (option.rect.height() - btnH) / 2,
        btnW,
        btnH
    );

    painter->setRenderHint(QPainter::Antialiasing, true);

    // Фон кнопки
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor("#E8F0F8"));
    painter->drawRoundedRect(btnRect, 3, 3);

    // Рамка кнопки
    QPen border(QColor("#005A9E"), 1);
    painter->setPen(border);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(btnRect, 3, 3);

    // Галочка ✓
    QPen checkPen(QColor("#2E7D32"), 2);
    checkPen.setCapStyle(Qt::RoundCap);
    checkPen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(checkPen);

    const QPointF c = btnRect.center();
    const QPointF p1(c.x() - 4, c.y() + 0.5);
    const QPointF p2(c.x() - 1, c.y() + 3);
    const QPointF p3(c.x() + 4, c.y() - 3);
    painter->drawLine(p1, p2);
    painter->drawLine(p2, p3);

    painter->restore();
}

QSize AlarmItemDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const {
    return QSize(-1, AlarmItemDelegate::ROW_HEIGHT);
}

bool AlarmItemDelegate::editorEvent(QEvent *event,
                                    QAbstractItemModel *model,
                                    const QStyleOptionViewItem &option,
                                    const QModelIndex &index)
{
    if (!index.isValid()) return false;

    if (event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() != Qt::LeftButton) {
            return QStyledItemDelegate::editorEvent(event, model, option, index);
        }

        // Зона кнопки (та же, что в paint()).
        const int btnH = qMin(option.rect.height() - 6, 18);
        const int btnW = ACK_BTN_WIDTH - 8;
        const QRect btnRect(
            option.rect.right() - ACK_BTN_WIDTH + 4,
            option.rect.top() + (option.rect.height() - btnH) / 2,
            btnW,
            btnH
        );

        if (btnRect.contains(me->pos())) {
            const int code = index.data(AlarmListModel::CodeRole).toInt();
            emit acknowledgeClicked(index.row(), code);
            return true;   // съели событие
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

// Реализация Панели (исправлены ошибки синтаксиса и имен)
AlarmPanel::AlarmPanel(QWidget *parent)
    : QWidget(parent),
      m_model(new AlarmListModel(this)),
      m_delegate(new AlarmItemDelegate(this))
{
    qDebug() << "[AlarmPanel] Инициализация (Оптимизированная версия)";

    m_alarmView = new QListView(this);
    m_alarmView->setFrameShape(QFrame::NoFrame);
    m_alarmView->setSelectionMode(QAbstractItemView::NoSelection);
    m_alarmView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_alarmView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_alarmView->viewport()->setAttribute(Qt::WA_StaticContents);
    m_alarmView->setUniformItemSizes(true);

    m_alarmView->setModel(m_model);
    m_alarmView->setItemDelegate(m_delegate);

    // подключаем сигнал делегата к слоту панели.
    connect(m_delegate, &AlarmItemDelegate::acknowledgeClicked,
            this, &AlarmPanel::onAcknowledgeClicked);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 6);
    layout->setSpacing(2);
    layout->addWidget(m_alarmView);

    setStyleSheet(
        "AlarmPanel { "
        " background-color: #F5F5F5; "
        " border: 1px solid black; "
        "}"
    );
}

AlarmPanel::~AlarmPanel() = default;

void AlarmPanel::setAlarms(const QList<int> &codes,
                           const QList<int> &statuses,
                           const QDateTime &cycleTime)
{
    QList<AlarmEntry> entries;
    entries.reserve(qMin(codes.size(), statuses.size()));

    for (int i = 0; i < codes.size() && i < statuses.size(); ++i) {
        // ✅ Используем переданное цикловое время, а не текущее системное.
        AlarmEntry e{codes[i], statuses[i], "", cycleTime, 0};
        entries.append(e);
    }

    m_model->setAlarms(entries);
    emit alarmCountChanged(m_model->criticalAlarmCount());
}

void AlarmPanel::clearAlarms() {
    m_model->setAlarms({});
    emit alarmCountChanged(0);
}

int AlarmPanel::criticalAlarmCount() const {
    return m_model->criticalAlarmCount();
}

int AlarmPanel::calculateHeight() const {
    if (m_model->rowCount() == 0) return 0;
    // ИСПРАВЛЕНО: Обращение к public-константе
    int contentHeight = m_model->rowCount() * AlarmItemDelegate::ROW_HEIGHT;
    int totalHeight = contentHeight + 10;
    return qMin(totalHeight, MAX_HEIGHT);
}

void AlarmPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    // 1. Заливаем фон всей панели серым цветом (#F5F5F5)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), QColor("#F5F5F5"));

    // 2. Получаем геометрию СПИСКА относительно ЭТОЙ ПАНЕЛИ
    // Так как список лежит внутри layout'а этой панели,
    // m_alarmView->geometry() УЖЕ возвращает координаты в системе координат AlarmPanel.
    QRect listGeo = m_alarmView->geometry();

    // 3. Рисуем рамку СНАРУЖИ (строго 1px)
    if (!listGeo.isEmpty()) {
        QPen pen(Qt::red);
        pen.setWidth(1);
        painter.setPen(pen);

        // Создаем внешний прямоугольник.
        // adjusted(-1, -1, 1, 1) расширяет listGeo на 1 пиксель во все стороны.
        // Теперь drawRect обведет именно эту внешнюю границу.
        QRect outerRect = listGeo.adjusted(-1, -1, 1, 1);

        painter.drawRect(outerRect);
    }
}

void AlarmPanel::onAcknowledgeClicked(int row, int code)
{
    Q_UNUSED(row);
    if (m_model->removeAlarmByCode(code)) {
        emit alarmCountChanged(m_model->criticalAlarmCount());
        emit alarmAcknowledged(code);
    }
}
