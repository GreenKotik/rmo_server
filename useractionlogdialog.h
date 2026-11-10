#ifndef USERACTIONLOGDIALOG_H
#define USERACTIONLOGDIALOG_H

#include <QDialog>
#include <QDateTimeEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>

// ============================================================================
// ✅ Диалог просмотра журнала действий пользователей.
// Повторяет логику CDialogGurnal из MFC-версии:
//   • два фильтра по дате (start/end) с чекбоксами;
//   • три фильтра-списка: пользователь, станция, операция;
//   • кнопка «Найти» пересобирает запрос;
//   • таблица 6 столбцов с автоподгонкой высоты строк по '\n'.
// ============================================================================
class UserActionLogDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UserActionLogDialog(QWidget *parent = nullptr);

private slots:
    void onFindClicked();

private:
    void setupUi();
    void loadFilterLists();
    void loadLog();

    // Виджеты фильтра
    QCheckBox     *m_checkTimeStart  = nullptr;
    QDateTimeEdit *m_dateStart       = nullptr;
    QCheckBox     *m_checkTimeEnd    = nullptr;
    QDateTimeEdit *m_dateEnd         = nullptr;
    QComboBox     *m_userFilter      = nullptr;
    QComboBox     *m_stationFilter   = nullptr;
    QComboBox     *m_operationFilter = nullptr;
    QPushButton   *m_findButton      = nullptr;

    // Таблица
    QTableWidget  *m_table           = nullptr;

    static constexpr int kMaxRows = 10000;   // ✅ максимум записей
};

#endif // USERACTIONLOGDIALOG_H
