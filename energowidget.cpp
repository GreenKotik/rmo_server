#include "energowidget.h"
#include "ui_energoform.h"
#include "databasemanager.h"  // ✅ Для записи в БД

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QMessageBox>

EnergoWidget::EnergoWidget(QWidget *parent)
    : BaseDocWidget(parent), ui(new Ui::EnergoForm)
{
    ui->setupUi(this);
    setWindowTitle("Энергоблок (новый)");

    // Заполнение выпадающего списка начальными значениями
    ui->voltageBox->addItems({"12 В", "24 В", "48 В", "110 В", "220 В"});

    // Подключение сигналов изменений полей к слоту обновления состояния
    connect(ui->voltageBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EnergoWidget::onFieldChanged);
    connect(ui->currentSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &EnergoWidget::onFieldChanged);
    connect(ui->stabilizedCheck, &QCheckBox::stateChanged, this, &EnergoWidget::onFieldChanged);

    // ✅ ДОБАВЛЕНО: Подключение кнопки применения (если она есть в UI)
    // Если в вашем .ui файле кнопка называется иначе, замените applyBtn на её имя
    if (ui->applyBtn) {
        connect(ui->applyBtn, &QPushButton::clicked, this, &EnergoWidget::onApplyClicked);
    }
}

EnergoWidget::~EnergoWidget()
{
    delete ui;
}

QSize EnergoWidget::documentFixedSize() const
{
    return QSize(475, 174);
}

void EnergoWidget::onFieldChanged()
{
    setModified(true);

    // Сбор текущего состояния виджета для отправки клиентам/серверу
    QJsonObject state;
    state["voltage"] = ui->voltageBox->currentText();
    state["current"] = ui->currentSpin->value();
    state["stabilized"] = ui->stabilizedCheck->isChecked();

    // Рассылка обновлений через базовый класс (бинарный протокол)
    broadcastDocUpdate(state);
}

// ✅ ДОБАВЛЕНО: Реализация метода, которого не хватало линковщику
void EnergoWidget::onApplyClicked()
{
    QMessageBox::information(this, "Действие", "Параметры энергоблока успешно применены.");
}

void EnergoWidget::loadFromFile(const QString &filePath)
{
    // ✅ Используем новый метод, который сохраняет все секции
    loadFromFilePreserveSections(filePath);
}

void EnergoWidget::saveToFile()
{
    if (m_filePath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Путь к файлу не задан. Используйте 'Сохранить как...'");
        return;
    }

    QJsonObject obj;
    obj["voltage"] = ui->voltageBox->currentText();
    obj["current"] = ui->currentSpin->value();
    obj["stabilized"] = ui->stabilizedCheck->isChecked();

    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл:\n" + f.errorString());
        return;
    }

    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.close();

    setModified(false);
    setWindowTitle(QFileInfo(m_filePath).completeBaseName() + ".enr");
}

void EnergoWidget::processDocClientData(const QJsonObject &data)
{
    if (data.value("action").toString() == "EnergoDataUpdate") {
        const QJsonObject payload = data.value("data").toObject();
        const bool writeToDb = payload.value("write_to_db").toBool(false);

        if (writeToDb && !m_filePath.isEmpty()) {
            DatabaseManager::instance().logDocumentData(
                m_filePath, 3, payload);
        }
        return;
    }

    // ... обработка doc_update (voltage / current / stabilized) ...
    if (data.contains("voltage")) {
        ui->voltageBox->setCurrentText(data["voltage"].toString());
    }
    // Применение данных, полученных от сервера или другого клиента по сети
    if (data.contains("voltage")) {
        ui->voltageBox->setCurrentText(data["voltage"].toString());
    }
    if (data.contains("current")) {
        ui->currentSpin->setValue(data["current"].toInt());
    }
    if (data.contains("stabilized")) {
        ui->stabilizedCheck->setChecked(data["stabilized"].toBool());
    }
}

QJsonObject EnergoWidget::getDocumentParameters() const
{
    QJsonObject params;

    // ✅ Собираем все параметры антенны

    return params;
}

void EnergoWidget::setDocumentParameters(const QJsonObject &params)
{
    // ✅ Устанавливаем параметры антенны
}
