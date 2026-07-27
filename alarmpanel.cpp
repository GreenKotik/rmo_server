#include "alarmpanel.h"
#include "alarmregistry.h"
#include <QDebug>

AlarmPanel::AlarmPanel(QWidget *parent)
    : QWidget(parent)
{
    // ✅ Настройка основного layout (без отступов сверху — панель является продолжением окна)
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 4, 6, 6);  // ✅ Те же отступы, что у документа
    m_mainLayout->setSpacing(2);

    // ✅ Контейнер для списка аварий
    m_alarmContainer = new QWidget(this);
    m_alarmLayout = new QVBoxLayout(m_alarmContainer);
    m_alarmLayout->setContentsMargins(0, 0, 0, 0);
    m_alarmLayout->setSpacing(2);

    m_mainLayout->addWidget(m_alarmContainer);

    // ✅ Стиль панели — такой же, как у окна документа (светло-серый фон)
    setStyleSheet(
        "AlarmPanel { "
        "  background-color: #F5F5F5; "
        "  border-top: 1px solid #B0B0B0; "  // ✅ Тонкая разделительная линия сверху
        "}"
    );

    qDebug() << "[AlarmPanel] Панель аварий создана (без заголовка)";
}

void AlarmPanel::setAlarms(const QList<int> &codes, const QList<int> &statuses)
{
    m_alarms.clear();

    for (int i = 0; i < codes.size() && i < statuses.size(); ++i) {
        int code = codes[i];
        int status = statuses[i];

        AlarmInfo info = AlarmRegistry::instance().getInfo(code);

        AlarmEntry entry;
        entry.code = code;
        entry.status = status;
        entry.message = info.mesRus;
        entry.timestamp = QDateTime::currentDateTime();
        entry.deviceId = 0;

        m_alarms.append(entry);
    }

    updateDisplay();

    qDebug() << "[AlarmPanel] Обновлён список аварий:" << m_alarms.size() << "шт.";
}

void AlarmPanel::clearAlarms()
{
    m_alarms.clear();
    updateDisplay();
    emit alarmCountChanged(0);
}

int AlarmPanel::criticalAlarmCount() const
{
    int count = 0;
    for (const AlarmEntry &entry : m_alarms) {
        if (entry.status == 1) {
            count++;
        }
    }
    return count;
}

int AlarmPanel::calculateHeight() const
{
    if (m_alarms.isEmpty()) {
        return 0;
    }

    // ✅ Высота = количество строк × высота строки + отступы
    int contentHeight = m_alarms.size() * ROW_HEIGHT;
    int totalHeight = contentHeight + 10;  // ✅ +10px на отступы layout

    // ✅ Ограничиваем максимальную высоту
    return qMin(totalHeight, MAX_HEIGHT);
}

void AlarmPanel::updateDisplay()
{
    rebuildAlarmList();
    emit alarmCountChanged(criticalAlarmCount());
}

void AlarmPanel::rebuildAlarmList()
{
    // Удаляем все виджеты аварий
    QLayoutItem *item;
    while ((item = m_alarmLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (m_alarms.isEmpty()) {
        return;
    }

    // Сортируем аварии: сначала критические (Status=1), потом информационные
    QList<AlarmEntry> sortedAlarms = m_alarms;
    std::sort(sortedAlarms.begin(), sortedAlarms.end(),
        [](const AlarmEntry &a, const AlarmEntry &b) {
            if (a.status != b.status) return a.status > b.status;
            return a.timestamp > b.timestamp;
        });

    // Добавляем каждую аварию
    for (const AlarmEntry &entry : sortedAlarms) {
        QWidget *alarmWidget = new QWidget(m_alarmContainer);
        QHBoxLayout *alarmLayout = new QHBoxLayout(alarmWidget);
        alarmLayout->setContentsMargins(4, 2, 4, 2);
        alarmLayout->setSpacing(8);

        // Цветовая индикация
        QColor bgColor = getAlarmColor(entry.status);
        alarmWidget->setStyleSheet(
            QString("background-color: %1; border-radius: 2px;").arg(bgColor.name())
        );
        alarmWidget->setFixedHeight(ROW_HEIGHT);

        // Время аварии
        QLabel *timeLabel = new QLabel(
            entry.timestamp.toString("HH:mm:ss"),
            alarmWidget
        );
        timeLabel->setStyleSheet(
            "font-family: 'Consolas', monospace; "
            "font-size: 11px; "
            "color: #424242; "
            "min-width: 60px;"
        );
        alarmLayout->addWidget(timeLabel);

        // Код аварии
        QLabel *codeLabel = new QLabel(
            QString("[%1]").arg(entry.code),
            alarmWidget
        );
        codeLabel->setStyleSheet(
            "font-family: 'Consolas', monospace; "
            "font-size: 11px; "
            "font-weight: bold; "
            "color: #616161; "
            "min-width: 50px;"
        );
        alarmLayout->addWidget(codeLabel);

        // Текст сообщения
        QLabel *messageLabel = new QLabel(entry.message, alarmWidget);
        messageLabel->setStyleSheet(
            QString("font-size: 15px; font-weight: 500; color: %1;").arg( // ✅ Увеличен шрифт до 14px
                entry.status == 1 ? "#C62828" : "#F57F17"
            )
        );
        // ✅ Гарантируем, что текст не переносится на новую строку, сохраняя высоту 24px
        messageLabel->setWordWrap(false);
        alarmLayout->addWidget(messageLabel, 1);

        m_alarmLayout->addWidget(alarmWidget);
    }
}

QColor AlarmPanel::getAlarmColor(int status) const
{
    switch (status) {
        case 1: return QColor("#FFEBEE");  // Красный фон для аварии
        case 0: return QColor("#FFF9C4");  // Жёлтый фон для информации
        case 2: return QColor("#E3F2FD");  // Синий фон для команды
        default: return QColor("#F5F5F5");
    }
}
