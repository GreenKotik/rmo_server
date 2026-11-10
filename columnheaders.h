#ifndef COLUMNHEADERS_H
#define COLUMNHEADERS_H

#include <QString>
#include <QColor>
#include <QHash>

// ============================================================================
// Реестр столбцов для DataViewDialog.
//
// Для каждого столбца БД задаёт:
//   • отображаемое имя (вместо имени из БД),
//   • начальную ширину столбца в пикселях,
//   • цвет для графика (фиксированный за каналом).
//
// Ключ — имя столбца в БД (например, "PerPower1").
// ============================================================================
class ColumnHeaders
{
public:
    struct Info {
        QString displayName;    // Человекочитаемое имя
        int     width = 0;      // Начальная ширина (0 = не задавать)
        QColor  color;          // Цвет для графика (пустой = авто)
    };

    // Получить информацию по имени столбца в БД.
    static Info get(const QString &dbName)
    {
        const auto &all = instance();
        auto it = all.constFind(dbName);
        if (it == all.constEnd()) {
            // Неизвестный столбец: имя = имя БД, ширина не задана,
            // цвет — из авто-палитры по хешу
            return Info{dbName, 0, autoColor(dbName)};
        }
        Info info = it.value();
        if (!info.color.isValid()) {
            info.color = autoColor(dbName);
        }
        return info;
    }

    // Удобные обёртки
    static QString displayName(const QString &dbName) { return get(dbName).displayName; }
    static int     width(const QString &dbName)       { return get(dbName).width; }
    static QColor  color(const QString &dbName)       { return get(dbName).color; }

private:
    // Автоматический цвет по хешу имени — стабилен между запусками
    static QColor autoColor(const QString &dbName)
    {
        static const QList<QColor> palette = {
            QColor(220,  50,  50),   // красный
            QColor( 50,  90, 220),   // синий
            QColor( 40, 160,  60),   // зелёный
            QColor(220, 150,  20),   // оранжево-жёлтый
            QColor(180,  60, 200),   // пурпурный
            QColor( 20, 180, 200),   // голубой
            QColor(150,  80,  30),   // коричневый
            QColor( 90,  90,  90),   // тёмно-серый
            QColor(230,  90, 160),   // розовый
            QColor(100, 180,  80),   // светло-зелёный
            QColor(140, 100, 200),   // сиреневый
            QColor(200, 200,  60),   // жёлтый
            QColor( 70, 140, 140),   // бирюзовый
            QColor(170, 170, 170),   // серый
            QColor(160,  30,  90),   // винный
            QColor( 30, 100, 160),   // тёмно-голубой
        };

        const uint h = qHash(dbName);
        return palette.at(h % static_cast<uint>(palette.size()));
    }

    static const QHash<QString, Info>& instance()
    {
        static const QHash<QString, Info> map = build();
        return map;
    }

    static QHash<QString, Info> build()
    {
        QHash<QString, Info> m;

        // Хелпер: добавить столбец
        auto add = [&m](const QString &dbName,
                        const QString &display,
                        int w,
                        const QColor &color = QColor()) {
            m.insert(dbName, Info{display, w, color});
        };

        // ====================================================================
        // Общие столбцы
        // ====================================================================
        add("id",           "№",              60);
        add("Дата_и_время", "Дата и время",  140);
        add("ErrorsAll",    "Аварийные сообщения",        200);

// QColor( 60,  90, 150)
// QColor(200,  40,  40)

        // ====================================================================
        // Передатчик (docType = 2)
        // ====================================================================
        add("Спутник",       "Спутник",       100);

        add("PerPowerIn1",   "P вх. УМ-1",     90, QColor(220,  50,  50));
        add("PerPower1",     "P вых. УМ-1",    90, QColor(220,  50,  50));
        add("PerPowerOtr1",  "P отр. УМ-1",    90, QColor(200, 120, 120));
        add("Attenuator1",   "Аттен. УМ-1",    80, QColor(160,  40,  40));
        add("Spiral1",       "Спираль УМ-1",   90, QColor(255, 100, 100));
        add("PerStatus1",    "Статус УМ-1",    80);

        add("PerPowerIn2",   "P вх. УМ-2",     90, QColor( 40, 160,  60));
        add("PerPower2",     "P вых. УМ-2",    90, QColor( 40, 160,  60));
        add("PerPowerOtr2",  "P отр. УМ-2",    90, QColor(120, 200, 120));
        add("Attenuator2",   "Аттен. УМ-2",    80, QColor( 30, 120,  40));
        add("Spiral2",       "Спираль УМ-2",   90, QColor(100, 220, 100));
        add("PerStatus2",    "Статус УМ-2",    80);

        add("PerPowerIn3",   "P вх. УМ-3",     90, QColor( 50,  90, 220));
        add("PerPower3",     "P вых. УМ-3",    90, QColor( 50,  90, 220));
        add("PerPowerOtr3",  "P отр. УМ-3",    90, QColor(130, 160, 230));
        add("Attenuator3",   "Аттен. УМ-3",    80, QColor( 30,  60, 170));
        add("Spiral3",       "Спираль УМ-3",   90, QColor(120, 160, 240));
        add("PerStatus3",    "Статус УМ-3",    80);

        add("SwitchPower",   "P суммарная",    90);
        add("SwitchStatus",  "Статус Switch",  80);
        add("SwitchReg",     "Режим Switch",   80);
        add("SwitchKanal",   "Канал Switch",   80);

        add("Temper1",       "Температура 1",  120, QColor(255, 140,   0));
        add("Temper2",       "Температура 2",  120, QColor(230,  90,  20));

        add("ProtokolPer1",  "Протокол УМ-1", 200);
        add("ProtokolPer2",  "Протокол УМ-2", 200);
        add("ProtokolPer3",  "Протокол УМ-3", 200);
        add("ProtokolPerSW", "Протокол Switch", 200);

        // ====================================================================
        // Антенна (docType = 1)
        // ====================================================================
        add("Азимут",           "Азимут",           80, QColor( 50,  90, 220));
        add("Угол_места",       "Угол места",       80, QColor(220,  50,  50));
        add("Угол_поляризации", "Угол поляризации",100, QColor( 40, 160,  60));
        add("Уровень_сигнала",  "Уровень сигнала", 100, QColor(255, 140,   0));

        add("Статус",           "Статус",          100);
        add("ОбогревТормозов",  "Обогрев тормозов",100);
        add("ПитаниеПриводов",  "Питание приводов",100);
        add("AntErrors",        "Ошибки антенны",  100);
        add("Komanda",          "Команда",         120);

        add("ProtokolKontroller", "Протокол контроллера",  200);
        add("ProtokolPilot",      "Протокол пилот-сигнала",200);
        add("ProtokolDatAzimut",  "Протокол датчика азимута", 200);
        add("ProtokolDatUgol",    "Протокол датчика угла",  200);
        add("ProtokolPosAzimut",  "Протокол позиционера азимута",200);
        add("ProtokolPosUgol",    "Протокол позиционера угла",  200);

        // ИБП-1
        add("IBP1_V",  "ИБП-1: U",  80, QColor(200,  40,  40));
        add("IBP1_A1", "ИБП-1: I1", 80, QColor(200,  40,  40));
        add("IBP1_W1", "ИБП-1: P1", 80, QColor(200,  40,  40));
        add("IBP1_A2", "ИБП-1: I2", 80, QColor(220,  80,  80));
        add("IBP1_W2", "ИБП-1: P2", 80, QColor(220,  80,  80));
        add("IBP1_A3", "ИБП-1: I3", 80, QColor(240, 120, 120));
        add("IBP1_W3", "ИБП-1: P3", 80, QColor(240, 120, 120));
        add("IBP1_A4", "ИБП-1: I4", 80);
        add("IBP1_W4", "ИБП-1: P4", 80);
        add("IBP1_A5", "ИБП-1: I5", 80);
        add("IBP1_W5", "ИБП-1: P5", 80);
        add("IBP1_A6", "ИБП-1: I6", 80);
        add("IBP1_W6", "ИБП-1: P6", 80);

        // ИБП-2
        add("IBP2_V",  "ИБП-2: U",  80, QColor( 40, 160,  60));
        add("IBP2_A1", "ИБП-2: I1", 80, QColor( 40, 160,  60));
        add("IBP2_W1", "ИБП-2: P1", 80, QColor( 40, 160,  60));
        add("IBP2_A2", "ИБП-2: I2", 80, QColor( 80, 200,  80));
        add("IBP2_W2", "ИБП-2: P2", 80, QColor( 80, 200,  80));
        add("IBP2_A3", "ИБП-2: I3", 80, QColor(120, 220, 120));
        add("IBP2_W3", "ИБП-2: P3", 80, QColor(120, 220, 120));
        add("IBP2_A4", "ИБП-2: I4", 80);
        add("IBP2_W4", "ИБП-2: P4", 80);
        add("IBP2_A5", "ИБП-2: I5", 80);
        add("IBP2_W5", "ИБП-2: P5", 80);
        add("IBP2_A6", "ИБП-2: I6", 80);
        add("IBP2_W6", "ИБП-2: P6", 80);

        // АВР
        for (int i = 1; i <= 8; ++i) {
            add(QString("AV_V%1").arg(i),
                QString("АВР: U%1").arg(i),
                80,
                QColor(150, 100, 200));
        }

        // Дискретные входы
        for (int i = 1; i <= 16; ++i) {
            add(QString("DV_D%1").arg(i),
                QString("ДВ-%1").arg(i),
                70);
        }

        // Температуры
        for (int i = 1; i <= 8; ++i) {
            // Оттенки оранжевого
            const int shade = 60 + i * 20;
            add(QString("Temperatura%1").arg(i),
                QString("Температура %1").arg(i),
                100,
                QColor(255, shade, 0));
        }

        // Обогрев
        for (int i = 1; i <= 12; ++i) {
            add(QString("Obogrev_V%1").arg(i),
                QString("Обогр. V%1").arg(i), 80, QColor(200, 150, 50));
            add(QString("Obogrev_A%1").arg(i),
                QString("Обогр. A%1").arg(i), 80, QColor(200, 150, 50));
            add(QString("Obogrev_W%1").arg(i),
                QString("Обогр. P%1").arg(i), 80, QColor(200, 150, 50));
        }

        // ====================================================================
        // Энерго (docType = 3)
        // ====================================================================
        static const QColor vvColors[4] = {
            QColor(200,  40,  40),   // ВВ-1 — красный
            QColor( 40, 160,  60),   // ВВ-2 — зелёный
            QColor( 50,  90, 220),   // ВВ-3 — синий
            QColor(255, 140,   0),   // ВВ-4 — оранжевый
        };

        for (int vv = 1; vv <= 4; ++vv) {
            const QColor c = vvColors[vv - 1];
            for (int i = 1; i <= 3; ++i) {
                add(QString("VV%1_In_V%2").arg(vv).arg(i),
                    QString("ВВ%1: U%2").arg(vv).arg(i), 90, c);
                add(QString("VV%1_In_A%2").arg(vv).arg(i),
                    QString("ВВ%1: I%2").arg(vv).arg(i), 90, c);
                add(QString("VV%1_In_W%2").arg(vv).arg(i),
                    QString("ВВ%1: P%2").arg(vv).arg(i), 90, c);
                add(QString("VV%1_Out_V%2").arg(vv).arg(i),
                    QString("ВВ%1 вых: U%2").arg(vv).arg(i), 90, c);
                add(QString("VV%1_Out_A%2").arg(vv).arg(i),
                    QString("ВВ%1 вых: I%2").arg(vv).arg(i), 90, c);
                add(QString("VV%1_Out_W%2").arg(vv).arg(i),
                    QString("ВВ%1 вых: P%2").arg(vv).arg(i), 90, c);
            }
            add(QString("VV%1_In_H").arg(vv),
                QString("ВВ%1: F").arg(vv), 70, c);
            add(QString("VV%1_In_AAll").arg(vv),
                QString("ВВ%1: I сум").arg(vv), 90, c);
            add(QString("VV%1_Out_H").arg(vv),
                QString("ВВ%1 вых: F").arg(vv), 80, c);
            add(QString("VV%1_Out_AAll").arg(vv),
                QString("ВВ%1 вых: I сум").arg(vv), 100, c);
        }

        for (int ibp = 1; ibp <= 3; ++ibp) {
            const int shade = 60 + ibp * 40;
            const QColor c(shade, 90, 200);
            for (const QString &f : {"V1", "V2", "V3", "A1", "A2", "A3",
                                     "W1", "W2", "W3"}) {
                add(QString("IBP%1_In_%2").arg(ibp).arg(f),
                    QString("ИБП-%1 вход: %2").arg(ibp).arg(f), 100, c);
                add(QString("IBP%1_Out_%2").arg(ibp).arg(f),
                    QString("ИБП-%1 выход: %2").arg(ibp).arg(f), 100, c);
                add(QString("IBP%1_Bypass_%2").arg(ibp).arg(f),
                    QString("ИБП-%1 байпас: %2").arg(ibp).arg(f), 100, c);
            }
            for (const QString &f : {"H", "AAll"}) {
                add(QString("IBP%1_In_%2").arg(ibp).arg(f),
                    QString("ИБП-%1 вход: %2").arg(ibp).arg(f), 90, c);
                add(QString("IBP%1_Out_%2").arg(ibp).arg(f),
                    QString("ИБП-%1 выход: %2").arg(ibp).arg(f), 90, c);
                add(QString("IBP%1_Bypass_%2").arg(ibp).arg(f),
                    QString("ИБП-%1 байпас: %2").arg(ibp).arg(f), 90, c);
            }
            add(QString("IBP%1_Bat_V").arg(ibp),
                QString("ИБП-%1 АКБ: U").arg(ibp), 100, c);
            add(QString("IBP%1_Bat_A").arg(ibp),
                QString("ИБП-%1 АКБ: I").arg(ibp), 100, c);
            add(QString("IBP%1_Bat_Zarad").arg(ibp),
                QString("ИБП-%1 АКБ: заряд").arg(ibp), 110, c);
        }

        add("ProtokolIBP1", "Протокол ИБП-1", 200);
        add("ProtokolIBP2", "Протокол ИБП-2", 200);
        add("ProtokolIBP3", "Протокол ИБП-3", 200);

        // ====================================================================
        // Модулятор (docType = 4)
        // ====================================================================
        add("usefulBitRate1", "Полезная скор. Мод-1", 140, QColor( 50,  90, 220));
        add("totalBitRate1",  "Общая скор. Мод-1",    130, QColor( 50,  90, 220));
        add("outputLevel1",   "Ур. выхода Мод-1",     120, QColor( 50,  90, 220));
        add("asiSyncLost1",   "ASI Мод-1 потерян",    130, QColor(200,  40,  40));
        add("rfOutputOk1",    "RF выход Мод-1",       120, QColor( 40, 160,  60));

        add("usefulBitRate2", "Полезная скор. Мод-2", 140, QColor(255, 140,   0));
        add("totalBitRate2",  "Общая скор. Мод-2",    130, QColor(255, 140,   0));
        add("outputLevel2",   "Ур. выхода Мод-2",     120, QColor(255, 140,   0));
        add("asiSyncLost2",   "ASI Мод-2 потерян",    130, QColor(200,  40,  40));
        add("rfOutputOk2",    "RF выход Мод-2",       120, QColor( 40, 160,  60));

        add("swMode",         "Режим Switch",         100);
        add("activeModulator","Активный модулятор",   140);
        add("power1On",       "Питание Мод-1",        110);
        add("power2On",       "Питание Мод-2",        110);
        add("swFault",        "Авария Switch",        110, QColor(200,  40,  40));

        return m;
    }
};

#endif // COLUMNHEADERS_H
