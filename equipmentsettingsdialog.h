#ifndef EQUIPMENTSETTINGSDIALOG_H
#define EQUIPMENTSETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QString>
#include <QVector>

class MainWindow;
class BaseDocWidget;
struct EquipmentConnection;

class EquipmentSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EquipmentSettingsDialog(QWidget *parent = nullptr);
    ~EquipmentSettingsDialog();

    void setDocumentFilePath(const QString &path);
    void setCurrentUser(const QString &username);

private slots:
    void onSave();

private:
    QTabWidget *tabWidget = nullptr;
    QSpinBox *pollingIntervalSpin = nullptr;
    QSpinBox *dbWriteIntervalSpin = nullptr;
    QCheckBox *inWorkCheck = nullptr;
    QSpinBox *alarmWriteDurationSpin = nullptr;  // ✅ НОВОЕ: запись при аварии (мин)

    QString m_documentFilePath;
    QString m_currentUser;

    // ✅ Чек "Опрос состояния оборудования" для вкладки датчика температуры
    QCheckBox *m_tempPollingCheck = nullptr;

    // ✅ Виджеты для настроек 4 датчиков температуры
    struct TemperatureSensorWidgets {
        QLineEdit *nameEdit = nullptr;
        QComboBox *typeCombo = nullptr;             // Тип датчика
        QDoubleSpinBox *popravkaSpin = nullptr;     // Поправка температуры
        QCheckBox *topPredelCheck = nullptr;        // Чек верхнего предела
        QDoubleSpinBox *topPredelSpin = nullptr;    // Значение верхнего предела
        QCheckBox *bottomPredelCheck = nullptr;     // Чек нижнего предела
        QDoubleSpinBox *bottomPredelSpin = nullptr; // Значение нижнего предела
    };
    QVector<TemperatureSensorWidgets> m_temperatureWidgets;

    // --- Методы ---
    BaseDocWidget* findDocWidget() const;
    void createPollingTab();
    void createEquipmentTabs();
    void createConnectionTab(const QString &name, const EquipmentConnection &conn);
    void createTemperatureSensorTab(const QString &name, const EquipmentConnection &conn);
    void loadSettings();
    void loadTemperatureSettings();
};

#endif // EQUIPMENTSETTINGSDIALOG_H
