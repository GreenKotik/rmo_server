#ifndef MODULATORCONTROLDIALOG_H
#define MODULATORCONTROLDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QTimer>
#include <QLabel>
#include <vector>
#include <memory>

class BaseDocWidget;
class ModulatorTabBase;
class LedIndicator;

// ============================================================================
// ✅ ГЛАВНЫЙ ДИАЛОГ УПРАВЛЕНИЯ МОДУЛЯТОРОМ
// Модальный, привязан к конкретному документу
// Заголовок: "Имя документа — Управление модулятором"
// ============================================================================
class ModulatorControlDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ModulatorControlDialog(BaseDocWidget *docWidget, QWidget *parent = nullptr);
    ~ModulatorControlDialog();

private slots:
    void onRefreshTimer();
    void onPollCycleStarted();  // Слот для синхронного мигания индикатора

private:
    BaseDocWidget *m_docWidget;
    QTabWidget *m_tabWidget;
    QTimer *m_refreshTimer;
    LedIndicator *m_pollIndicator;  // Индикатор опроса в заголовке

    std::vector<ModulatorTabBase*> m_tabs;

    // Создаём табы в зависимости от подключённых устройств
    void createTabs();
};

#endif // MODULATORCONTROLDIALOG_H
