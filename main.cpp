#include "mainwindow.h"
#include <QApplication>
#include <QTextCodec>
#include <QCommandLineParser>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDebug>
#include <QSqlDatabase>
#include <QPair>
#include <QVector>
#include "devicedata.h"
#include <memory>
#include "measurementsmodel.h"

int main(int argc, char *argv[])
{
    qDebug() << "[Main] Запуск приложения...";
    // ✅ КРИТИЧНО для Linux: имена файлов на кириллице в messages/
    //    должны корректно конвертироваться QString ↔ ФС.
    //    На Windows безвредно.
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
    QApplication a(argc, argv);

    qRegisterMetaType<MeasurementsModel::Row>("MeasurementsModel::Row");
    qRegisterMetaType<QVector<MeasurementsModel::Row>>("QVector<MeasurementsModel::Row>");
    qRegisterMetaType<ControlCommand>("ControlCommand");

    // Проверка доступных драйверов:
    qDebug() << "Available SQL drivers:" << QSqlDatabase::drivers();
    qDebug() << "Library paths:" << QCoreApplication::libraryPaths();

    // ✅ РЕГИСТРАЦИЯ МЕТАТИПОВ для передачи между потоками
    qRegisterMetaType<std::shared_ptr<IDeviceData>>("std::shared_ptr<IDeviceData>");
    qRegisterMetaType<ControlCommand>("ControlCommand");

    // ✅ КРИТИЧЕСКИ ВАЖНО: DbLoaderWorker::finished передаёт QVector<QPair<double,double>>.
    // Без регистрации этих типов Qt молча теряет сигнал через QueuedConnection —
    // прогресс доходит до 100%, а onDbLoadFinished() не вызывается.
    qRegisterMetaType<QPair<double, double>>("QPair<double,double>");
    qRegisterMetaType<QVector<QPair<double, double>>>("QVector<QPair<double,double>>");

    QCommandLineParser parser;
    parser.setApplicationDescription("RMO Server/Client");
    parser.addHelpOption();
    parser.addOptions({
        {"mode", "Run mode: server or client", "mode"},
        {"config", "Path to config file", "config", "rmo_config.json"}
    });
    parser.process(a);

    QString configPath = parser.value("config");
    if (!QFile::exists(configPath)) {
        configPath = QCoreApplication::applicationDirPath() + "/rmo_config.json";
    }

    // ✅ Загружаем конфигурацию приложения
    QJsonObject appConfig;
    if (QFile::exists(configPath)) {
        QFile f(configPath);
        if (f.open(QIODevice::ReadOnly)) {
            appConfig = QJsonDocument::fromJson(f.readAll()).object();
        }
    }

    // ✅ Определяем режим: сначала из аргументов, потом из конфига
    QString modeStr = parser.value("mode").toLower();
    if (modeStr.isEmpty()) {
        modeStr = appConfig.value("mode").toString("server");
    }
    AppMode mode = (modeStr == "client") ? AppMode::Client : AppMode::Server;

    // ✅ Формируем конфиг клиента
    QJsonObject clientConfig;
    if (mode == AppMode::Client) {
        clientConfig["server_ip"] = appConfig.value("clientServerIp").toString("127.0.0.1");
        clientConfig["server_port"] = appConfig.value("clientServerPort").toInt(5000);
    }

    qDebug() << "[Main] Режим:" << modeStr;
    qDebug() << "[Main] Название программы:" << appConfig.value("programName").toString("RMO");

    MainWindow w(mode, clientConfig);
    w.show();

    qDebug() << "[Main] MainWindow создан и показан";
    qDebug() << "[Main] Запуск event loop...";

    int result = a.exec();

    qDebug() << "[Main] Event loop завершён с кодом:" << result;
    qDebug() << "[Main] Начинается уничтожение объектов...";

    return result;
}
