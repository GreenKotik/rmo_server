#ifndef ALARMSOUNDPLAYER_H
#define ALARMSOUNDPLAYER_H

#include <QObject>
#include <QStringList>
#include <QMap>
#include <QQueue>
#include <QMediaPlayer>
#include <QMediaContent>
#include <QUrl>

// ============================================================================
// ✅ Глобальный проигрыватель звуков аварий (singleton).
//
// Логика:
//  • Каждый документ при каждом цикле опроса регистрирует свои активные
//    (не подтверждённые) аварии через registerDocAlarms(docName, texts, enabled).
//  • Плеер играет «фразы»: <docName>.wav → <alarmText>.wav
//    (или default.wav вместо <docName>.wav, если файла нет).
//  • Фразы чередуются между документами (round-robin).
//  • Внутри одного документа — по кругу, пока авария не подтверждена.
//  • Ничего не накладывается: пока одна фраза не доиграла — следующая
//    не стартует.
//  • Документ можно «выключить» (enabled=false) — его фразы пропускаются.
// ============================================================================
class AlarmSoundPlayer : public QObject
{
    Q_OBJECT
public:
    // ✅ Singleton.
    static AlarmSoundPlayer& instance();

    // Папка с .wav-файлами.
    void setMessagesDir(const QString &dirPath);
    QString messagesDir() const { return m_messagesDir; }

    // ✅ Регистрация активных аварий документа.
    //    Вызывается КАЖДЫЙ цикл опроса (не только при изменении).
    //    Пустой список = «у документа нет активных аварий» (документ
    //    удаляется из очереди). enabled=false = документ «выключен»
    //    (его фразы не играются, но он остаётся в очереди).
    void registerDocAlarms(const QString &docName,
                           const QStringList &alarmTexts,
                           bool enabled);

    // ✅ Убрать документ полностью (при закрытии).
    void unregisterDoc(const QString &docName);

    // Полная остановка и очистка очереди.
    void stopAll();

    // Глобальное включение/выключение (для будущих нужд).
    void setEnabled(bool enabled) { m_enabled = enabled; if (!m_enabled) stopAll(); }
    bool isEnabled() const { return m_enabled; }

private slots:
    void onMediaStateChanged(QMediaPlayer::State state);

private:
    explicit AlarmSoundPlayer(QObject *parent = nullptr);
    ~AlarmSoundPlayer() override;

    // ✅ Внутреннее состояние по каждому документу.
    struct DocQueue {
        QStringList texts;   // тексты активных аварий
        int nextIdx = 0;     // следующая авария (по кругу)
        bool enabled = true; // играть ли фразы этого документа
    };

    // Взять следующую «фразу» (docName + alarmText) по round-robin.
    // Возвращает false, если нечего играть.
    bool pickNextPhrase(QString &outDocName, QString &outAlarmText);

    // Проиграть следующий файл из m_playingQueue.
    void playCurrentFile();

    // Стартовать следующую фразу (вызывается, когда очередь файлов пуста).
    void tryPlayNext();

    // Путь к <text>.wav в папке messages/.
    QString filePathForText(const QString &text) const;

    QString m_messagesDir;
    bool m_enabled = true;

    // Документы и их очереди.
    QMap<QString, DocQueue> m_docQueues;
    // Порядок документов для round-robin (без повторов).
    QStringList m_roundRobinOrder;

    // Текущая «фраза»: очередь файлов, которые надо сыграть подряд.
    QQueue<QString> m_playingQueue;
    bool m_playing = false;

    // Для отладки/логов.
    QString m_currentDocName;
    QString m_currentAlarmText;

    QMediaPlayer *m_player = nullptr;
};

#endif // ALARMSOUNDPLAYER_H
