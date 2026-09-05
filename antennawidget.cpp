#include "antennawidget.h"
#include "ui_antennaform.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QMessageBox>

AntennaWidget::AntennaWidget(QWidget *parent)
    : BaseDocWidget(parent), ui(new Ui::AntennaForm)
{
    ui->setupUi(this);
    setWindowTitle("Антенна (новый)");

    // Заполнение выпадающих списков начальными значениями
    ui->gainBox->addItems({"10 dB", "15 dB", "20 dB", "25 dB"});
    ui->polarizationBox->addItems({"Вертикальная", "Горизонтальная", "Круговая"});

    // Подключение сигналов изменений полей к слоту обновления состояния
    connect(ui->nameEdit, &QLineEdit::textChanged, this, &AntennaWidget::onFieldChanged);
    connect(ui->gainBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AntennaWidget::onFieldChanged);
    connect(ui->polarizationBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AntennaWidget::onFieldChanged);
    connect(ui->isMounted, &QCheckBox::stateChanged, this, &AntennaWidget::onFieldChanged);

    // Кнопка применения (может выполнять локальную логику или просто подтверждать)
    connect(ui->applyBtn, &QPushButton::clicked, this, &AntennaWidget::onApplyClicked);
}

AntennaWidget::~AntennaWidget()
{
    delete ui;
}

QSize AntennaWidget::documentFixedSize() const
{
    return QSize(475, 174);
}

void AntennaWidget::onFieldChanged()
{
    setModified(true);

    // Сбор текущего состояния виджета для отправки клиентам/серверу
    QJsonObject state;
    state["name"] = ui->nameEdit->text();
    state["gain"] = ui->gainBox->currentText();
    state["polarization"] = ui->polarizationBox->currentText();
    state["mounted"] = ui->isMounted->isChecked();

    // Рассылка обновлений через базовый класс
    broadcastDocUpdate(state);
}

void AntennaWidget::onApplyClicked()
{
    QMessageBox::information(this, "Действие", "Параметры антенны успешно применены.");
}

void AntennaWidget::loadFromFile(const QString &filePath)
{
    // ✅ Используем новый метод, который сохраняет все секции
    loadFromFilePreserveSections(filePath);
}

void AntennaWidget::saveToFile()
{
    if (m_filePath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Путь к файлу не задан. Используйте 'Сохранить как...'");
        return;
    }

    QJsonObject obj;
    obj["name"] = ui->nameEdit->text();
    obj["gain"] = ui->gainBox->currentText();
    obj["polarization"] = ui->polarizationBox->currentText();
    obj["mounted"] = ui->isMounted->isChecked();

    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл:\n" + f.errorString());
        return;
    }

    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.close();

    setModified(false);
    setWindowTitle(QFileInfo(m_filePath).completeBaseName() + ".ant");
}

void AntennaWidget::processDocClientData(const QJsonObject &data)
{
    // Применение данных, полученных от сервера или другого клиента по сети
    // Важно: обновляем UI только если данные действительно пришли, чтобы не сбрасывать их в пустоту
    if (data.contains("name")) {
        ui->nameEdit->setText(data["name"].toString());
    }
    if (data.contains("gain")) {
        ui->gainBox->setCurrentText(data["gain"].toString());
    }
    if (data.contains("polarization")) {
        ui->polarizationBox->setCurrentText(data["polarization"].toString());
    }
    if (data.contains("mounted")) {
        ui->isMounted->setChecked(data["mounted"].toBool());
    }
}

QJsonObject AntennaWidget::getDocumentParameters() const
{
    QJsonObject params;

    // ✅ Собираем все параметры антенны

    return params;
}

void AntennaWidget::setDocumentParameters(const QJsonObject &params)
{
    // ✅ Устанавливаем параметры антенны
}
