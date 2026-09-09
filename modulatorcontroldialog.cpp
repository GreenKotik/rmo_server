#include "modulatorcontroldialog.h"
#include "modulatortab.h"
#include "switchtab.h"
#include "stubtab.h"
#include "basedocwidget.h"
#include "ledindicator.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QDebug>

ModulatorControlDialog::ModulatorControlDialog(BaseDocWidget *docWidget, QWidget *parent)
    : QDialog(parent)
    , m_docWidget(docWidget)
    , m_refreshTimer(new QTimer(this))
{
    // Настройки окна
    setModal(true);
    setWindowTitle(QFileInfo(m_docWidget->filePath()).completeBaseName() + " — Управление модулятором");
    setFixedSize(1000, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // === Заголовок с индикатором опроса ===
    QHBoxLayout *headerLayout = new QHBoxLayout();

    QLabel *headerLabel = new QLabel("<b>Управление модулятором</b>", this);
    headerLabel->setStyleSheet("font-size: 16px; color: #005A9E;");
    headerLayout->addWidget(headerLabel);

    headerLayout->addStretch();

    QLabel *pollLabel = new QLabel("Опрос:", this);
    pollLabel->setStyleSheet("color: #555555;");
    headerLayout->addWidget(pollLabel);

    m_pollIndicator = new LedIndicator(this);
    m_pollIndicator->setFixedSize(16, 16);
    m_pollIndicator->setState(LedIndicator::State::Gray);
    headerLayout->addWidget(m_pollIndicator);

    mainLayout->addLayout(headerLayout);

    // === QTabWidget ===
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #D0D0D0; background: white; }"
        "QTabBar::tab { padding: 8px 16px; }"
        "QTabBar::tab:selected { background: #005A9E; color: white; }"
    );
    mainLayout->addWidget(m_tabWidget);

    // === Создаём табы ===
    createTabs();

    // === Таймер обновления данных ===
    // Читаем интервал опроса из настроек документа
    int interval = m_docWidget->getSettings().value("polling_interval").toInt(1000);
    if (interval < 100) interval = 100;
    m_refreshTimer->setInterval(interval);
    connect(m_refreshTimer, &QTimer::timeout, this, &ModulatorControlDialog::onRefreshTimer);
    m_refreshTimer->start();

    // === Подключаем сигнал начала цикла опроса для синхронного мигания ===
    connect(m_docWidget, &BaseDocWidget::pollCycleStarted,
            this, &ModulatorControlDialog::onPollCycleStarted);

    // Первичное обновление
    onRefreshTimer();

    qDebug() << "[ModulatorControlDialog] Диалог создан для:" << m_docWidget->filePath();
}

ModulatorControlDialog::~ModulatorControlDialog()
{
    qDebug() << "[ModulatorControlDialog] Диалог закрыт";
}

void ModulatorControlDialog::createTabs()
{
    auto connections = m_docWidget->getEquipmentConnections();

    // Индексы строк в m_equipmentConnections для Modulator:
    // 0 - Усилитель-распределитель
    // 1 - Модулятор 1
    // 2 - Модулятор 2
    // 3 - Переключатель резервирования
    // 4 - Анализатор потока

    // === Таб "Модулятор 1" (индекс 1) ===
    if (connections.size() > 1 && connections[1].connected) {
        auto *tab = new ModulatorTab(m_docWidget, 1, this);
        m_tabWidget->addTab(tab, "Спутниковый модулятор 1");
        m_tabs.push_back(tab);
    }

    // === Таб "Модулятор 2" (индекс 2) ===
    if (connections.size() > 2 && connections[2].connected) {
        auto *tab = new ModulatorTab(m_docWidget, 2, this);
        m_tabWidget->addTab(tab, "Спутниковый модулятор 2");
        m_tabs.push_back(tab);
    }

    // === Таб "Переключатель" (индекс 3) ===
    if (connections.size() > 3 && connections[3].connected) {
        auto *tab = new SwitchTab(m_docWidget, this);
        m_tabWidget->addTab(tab, "Переключатель резервирования");
        m_tabs.push_back(tab);
    }

    // === Таб "Усилитель" (индекс 0) — заглушка ===
    if (connections.size() > 0 && connections[0].connected) {
        auto *tab = new StubTab(m_docWidget, "Усилитель-распределитель", this);
        m_tabWidget->addTab(tab, "Усилитель-распределитель");
        m_tabs.push_back(tab);
    }

    // === Таб "Анализатор" (индекс 4) — заглушка ===
    if (connections.size() > 4 && connections[4].connected) {
        auto *tab = new StubTab(m_docWidget, "Анализатор потока", this);
        m_tabWidget->addTab(tab, "Анализатор потока");
        m_tabs.push_back(tab);
    }

    // Если ни одного таба не создано — показываем сообщение
    if (m_tabs.empty()) {
        auto *stub = new StubTab(m_docWidget, "Нет подключённых устройств", this);
        m_tabWidget->addTab(stub, "Информация");
    }
}

void ModulatorControlDialog::onRefreshTimer()
{
    for (auto *tab : m_tabs) {
        if (tab) tab->refreshData();
    }
}

void ModulatorControlDialog::onPollCycleStarted()
{
    // ✅ Синхронное мигание индикатора с документом
    if (m_pollIndicator) {
        m_pollIndicator->trigger(300);
    }
}
