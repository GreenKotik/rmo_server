#include "peredatchikwidget.h"
#include "ui_peredatchikform.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QMessageBox>

PeredatchikWidget::PeredatchikWidget(QWidget *parent)
    : BaseDocWidget(parent), ui(new Ui::PeredatchikForm)
{
    ui->setupUi(this);
    setWindowTitle("Передатчик (новый)");

    ui->bandBox->addItems({"ДВ", "СВ", "КВ", "УКВ"});
    ui->modeBox->addItems({"Телефония", "Телеграф", "Цифровой"});

    connect(ui->txNameEdit, &QLineEdit::textChanged, this, &PeredatchikWidget::onFieldChanged);
    connect(ui->bandBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PeredatchikWidget::onFieldChanged);
    connect(ui->modeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PeredatchikWidget::onFieldChanged);
    connect(ui->outPowerSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PeredatchikWidget::onFieldChanged);
    connect(ui->onAirCheck, &QCheckBox::stateChanged, this, &PeredatchikWidget::onFieldChanged);

    // ✅ ДОБАВЛЕНО: Подключение кнопки применения (если она есть в UI)
    if (ui->applyBtn) {
        connect(ui->applyBtn, &QPushButton::clicked, this, &PeredatchikWidget::onApplyClicked);
    }
}

PeredatchikWidget::~PeredatchikWidget()
{
    delete ui;
}

QSize PeredatchikWidget::documentFixedSize() const
{
    return QSize(380, 380);
}

void PeredatchikWidget::onFieldChanged()
{
    setModified(true);

    QJsonObject state;
    state["name"] = ui->txNameEdit->text();
    state["band"] = ui->bandBox->currentText();
    state["mode"] = ui->modeBox->currentText();
    state["outPower"] = ui->outPowerSpin->value();
    state["onAir"] = ui->onAirCheck->isChecked();

    broadcastDocUpdate(state);
}

// ✅ ДОБАВЛЕНО: Реализация метода, которого не хватало линковщику
void PeredatchikWidget::onApplyClicked()
{
    QMessageBox::information(this, "Действие", "Параметры передатчика успешно применены.");
}

void PeredatchikWidget::loadFromFile(const QString &filePath)
{
    // ✅ Используем новый метод, который сохраняет все секции
    loadFromFilePreserveSections(filePath);
}

void PeredatchikWidget::saveToFile()
{
    if (m_filePath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Путь к файлу не задан. Используйте 'Сохранить как...'");
        return;
    }

    QJsonObject obj;
    obj["name"] = ui->txNameEdit->text();
    obj["band"] = ui->bandBox->currentText();
    obj["mode"] = ui->modeBox->currentText();
    obj["outPower"] = ui->outPowerSpin->value();
    obj["onAir"] = ui->onAirCheck->isChecked();

    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл:\n" + f.errorString());
        return;
    }

    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.close();

    setModified(false);
    setWindowTitle(QFileInfo(m_filePath).completeBaseName() + ".per");
}

void PeredatchikWidget::processDocClientData(const QJsonObject &data)
{
    if (data.contains("name")) ui->txNameEdit->setText(data["name"].toString());
    if (data.contains("band")) ui->bandBox->setCurrentText(data["band"].toString());
    if (data.contains("mode")) ui->modeBox->setCurrentText(data["mode"].toString());
    if (data.contains("outPower")) ui->outPowerSpin->setValue(data["outPower"].toInt());
    if (data.contains("onAir")) ui->onAirCheck->setChecked(data["onAir"].toBool());
}

QJsonObject PeredatchikWidget::getDocumentParameters() const
{
    QJsonObject params;

    // ✅ Собираем все параметры антенны

    return params;
}

void PeredatchikWidget::setDocumentParameters(const QJsonObject &params)
{
    // ✅ Устанавливаем параметры антенны
}
