#include "peredatchikwidget.h"
#include "devicedata.h"  // ✅ Для доступа к Boreas3Data
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <memory>

PeredatchikWidget::PeredatchikWidget(QWidget *parent)
    : BaseDocWidget(parent), ui(new Ui::PeredatchikForm)
{
    ui->setupUi(this);
    // Инициализация UI нулями или прочерками
    ui->labelTemp1->setText("Температура 1: --.- °C");
    ui->labelTemp2->setText("Температура 2: --.- °C");
    ui->labelTemp3->setText("Температура 3: --.- °C");
    ui->labelTemp4->setText("Температура 4: --.- °C");
}

PeredatchikWidget::~PeredatchikWidget()
{
    delete ui;
}

QSize PeredatchikWidget::documentFixedSize() const
{
    return QSize(475, 174);
}

void PeredatchikWidget::loadFromFile(const QString &path)
{
    loadFromFilePreserveSections(path);
}

void PeredatchikWidget::saveToFile()
{
    saveToFilePreserveSections(filePath());
}

void PeredatchikWidget::processDocClientData(const QJsonObject &data)
{
    // Здесь можно добавить обработку команд от клиента, если потребуется
    Q_UNUSED(data);
}


// ✅ РЕАЛИЗАЦИЯ ПАКЕТНОГО ОБНОВЛЕНИЯ UI (Двойная буферизация)
void PeredatchikWidget::updateUIFromBuffer()
{
    // ✅ Получаем унифицированные данные для 12-го слота (индекс 11)
    auto tempData = getTemperatureDataForSlot(11);
    if (!tempData) {
        // Если данные ещё не получены — показываем прочерки
        ui->labelTemp1->setText("Температура 1: --.- °C");
        ui->labelTemp2->setText("Температура 2: --.- °C");
        ui->labelTemp3->setText("Температура 3: --.- °C");
        ui->labelTemp4->setText("Температура 4: --.- °C");
        return;
    }

    // ✅ Обновляем UI из унифицированной структуры
    // Отображаем только первые 4 датчика (для Boreas 3 и TE Monitor V6)
    ui->labelTemp1->setText(QString("Температура 1: %1 °C").arg(tempData->temperatures[0], 0, 'f', 1));
    ui->labelTemp2->setText(QString("Температура 2: %1 °C").arg(tempData->temperatures[1], 0, 'f', 1));
    ui->labelTemp3->setText(QString("Температура 3: %1 °C").arg(tempData->temperatures[2], 0, 'f', 1));
    ui->labelTemp4->setText(QString("Температура 4: %1 °C").arg(tempData->temperatures[3], 0, 'f', 1));

    // ✅ Опционально: можно показать статусы датчиков (если есть аварии)
    // Например, менять цвет текста при alarms[i] != 0
}
