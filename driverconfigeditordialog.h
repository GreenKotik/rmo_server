#ifndef DRIVERCONFIGEDITORDIALOG_H
#define DRIVERCONFIGEDITORDIALOG_H

#include <QDialog>
#include <QStringList>
#include "driverconfigmodel.h"

class QListWidget;
class QTabWidget;
class QLineEdit;
class QTableWidget;
class QTextEdit;
class QPushButton;
class QSpinBox;
class QComboBox;

// ============================================================================
// ✅ РЕДАКТОР КОНФИГОВ ДРАЙВЕРОВ
//
// Слева — список *.json из applicationDirPath()/drivers/modulators/.
// Справа — три вкладки: Общее / Параметры / Правила аварий.
// Внизу — панель валидации (список проблем).
//
// Сохранение логируется через DatabaseManager::logUserAction.
// Право: "edit_driver_config".
// ============================================================================
class DriverConfigEditorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DriverConfigEditorDialog(QWidget *parent = nullptr);
    ~DriverConfigEditorDialog() override;

private slots:
    void onFileSelected(int row);
    void onAddParam();
    void onRemoveParam();
    void onAddRule();
    void onRemoveRule();
    void onSave();
    void onSaveAs();
    void onReload();
    void onAddDeviceId();
    void onRemoveDeviceId();

private:
    QString configDir() const;
    void refreshFileList();
    void loadCurrentFile(const QString &path);
    void refreshParamsTable();
    void refreshRulesTable();
    void refreshDeviceIds();
    void runValidation();
    void updateUiFromModel();
    bool syncUiToModel();   // вернёт false, если ввод кривой (например, не-число)
    bool ensureSaved();

    // Модель
    DriverConfigModel m_model;
    bool m_dirty = false;
    bool m_loading = false;   // защита от рекурсии при заполнении таблиц

    // UI
    QListWidget    *m_fileList = nullptr;
    QTabWidget     *m_tabs = nullptr;

    // Общее
    QLineEdit      *m_modelEdit = nullptr;
    QListWidget    *m_deviceIdsList = nullptr;
    QSpinBox       *m_deviceIdSpin = nullptr;
    QPushButton    *m_addDeviceIdBtn = nullptr;
    QPushButton    *m_removeDeviceIdBtn = nullptr;

    // Параметры
    QTableWidget   *m_paramsTable = nullptr;
    QPushButton    *m_addParamBtn = nullptr;
    QPushButton    *m_removeParamBtn = nullptr;

    // Правила
    QTableWidget   *m_rulesTable = nullptr;
    QPushButton    *m_addRuleBtn = nullptr;
    QPushButton    *m_removeRuleBtn = nullptr;

    // Валидация
    QTextEdit      *m_problems = nullptr;

    // Кнопки
    QPushButton    *m_saveBtn = nullptr;
    QPushButton    *m_saveAsBtn = nullptr;
    QPushButton    *m_reloadBtn = nullptr;
    QPushButton    *m_closeBtn = nullptr;
};

#endif // DRIVERCONFIGEDITORDIALOG_H
