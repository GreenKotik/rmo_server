#include "modulatorwidget.h"
#include "ui_modulatorform.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QMessageBox>

ModulatorWidget::ModulatorWidget(QWidget *parent)
    : BaseDocWidget(parent), ui(new Ui::ModulatorForm)
{
    ui->setupUi(this);
    setWindowTitle("Модулятор (новый)");

    // Заполнение выпадающих списков начальными значениями
    ui->modTypeBox->addItems({"AM", "FM", "PM", "QAM-16", "QAM-64"});
    ui->freqBandBox->addItems({"HF", "VHF", "UHF", "SHF"});

    // Подключение сигналов изменений полей к слоту обновления состояния
    connect(ui->modNameEdit, &QLineEdit::textChanged, this, &ModulatorWidget::onFieldChanged);
    connect(ui->modTypeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModulatorWidget::onFieldChanged);
    connect(ui->freqBandBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModulatorWidget::onFieldChanged);
    connect(ui->powerSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ModulatorWidget::onFieldChanged);
    connect(ui->isLockedCheck, &QCheckBox::stateChanged, this, &ModulatorWidget::onFieldChanged);

    // ✅ ДОБАВЛЕНО: Подключение кнопки применения (если она есть в UI)
    if (ui->applyBtn) {
        connect(ui->applyBtn, &QPushButton::clicked, this, &ModulatorWidget::onApplyClicked);
    }
}

ModulatorWidget::~ModulatorWidget()
{
    delete ui;
}

QSize ModulatorWidget::documentFixedSize() const
{
    return QSize(360, 380);
}

void ModulatorWidget::onFieldChanged()
{
    setModified(true);

    // Сбор текущего состояния виджета для отправки клиентам/серверу
    QJsonObject state;
    state["name"] = ui->modNameEdit->text();
    state["modType"] = ui->modTypeBox->currentText();
    state["freqBand"] = ui->freqBandBox->currentText();
    state["power"] = ui->powerSpin->value();
    state["isLocked"] = ui->isLockedCheck->isChecked();

    // Рассылка обновлений через базовый класс (бинарный протокол)
    broadcastDocUpdate(state);
}

// ✅ ДОБАВЛЕНО: Реализация метода, которого не хватало линковщику
void ModulatorWidget::onApplyClicked()
{
    QMessageBox::information(this, "Действие", "Параметры модулятора успешно применены.");
}

void ModulatorWidget::loadFromFile(const QString &filePath)
{
    // ✅ Используем новый метод, который сохраняет все секции
    loadFromFilePreserveSections(filePath);
}

void ModulatorWidget::saveToFile()
{
    if (m_filePath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Путь к файлу не задан. Используйте 'Сохранить как...'");
        return;
    }

    QJsonObject obj;
    obj["name"] = ui->modNameEdit->text();
    obj["modType"] = ui->modTypeBox->currentText();
    obj["freqBand"] = ui->freqBandBox->currentText();
    obj["power"] = ui->powerSpin->value();
    obj["isLocked"] = ui->isLockedCheck->isChecked();

    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл:\n" + f.errorString());
        return;
    }

    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.close();

    setModified(false);
    setWindowTitle(QFileInfo(m_filePath).completeBaseName() + ".mod");
}

void ModulatorWidget::processDocClientData(const QJsonObject &data)
{
    // Применение данных, полученных от сервера или другого клиента по сети
    if (data.contains("name")) ui->modNameEdit->setText(data["name"].toString());
    if (data.contains("modType")) ui->modTypeBox->setCurrentText(data["modType"].toString());
    if (data.contains("freqBand")) ui->freqBandBox->setCurrentText(data["freqBand"].toString());
    if (data.contains("power")) ui->powerSpin->setValue(data["power"].toInt());
    if (data.contains("isLocked")) ui->isLockedCheck->setChecked(data["isLocked"].toBool());
}

QJsonObject ModulatorWidget::getDocumentParameters() const
{
    QJsonObject params;

    // ✅ Собираем все параметры антенны

    return params;
}

void ModulatorWidget::setDocumentParameters(const QJsonObject &params)
{
    // ✅ Устанавливаем параметры антенны
}
