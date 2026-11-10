#include "alarmsoundplayer.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

// ============================================================================
// SINGLETON
// ============================================================================

AlarmSoundPlayer& AlarmSoundPlayer::instance()
{
    static AlarmSoundPlayer s;
    return s;
}

AlarmSoundPlayer::AlarmSoundPlayer(QObject *parent)
    : QObject(parent)
{
    m_messagesDir = QCoreApplication::applicationDirPath() + "/messages";

    m_player = new QMediaPlayer(this);
    m_player->setVolume(100);

    connect(m_player, &QMediaPlayer::stateChanged,
            this, &AlarmSoundPlayer::onMediaStateChanged);
}

AlarmSoundPlayer::~AlarmSoundPlayer()
{
    stopAll();
}

// ============================================================================
// НАСТРОЙКИ
// ============================================================================

void AlarmSoundPlayer::setMessagesDir(const QString &dirPath)
{
    m_messagesDir = dirPath;
}

QString AlarmSoundPlayer::filePathForText(const QString &text) const
{
    QDir dir(m_messagesDir);
    return dir.absoluteFilePath(text + ".wav");
}

// ============================================================================
// РЕГИСТРАЦИЯ / СНЯТИЕ
// ============================================================================

void AlarmSoundPlayer::registerDocAlarms(const QString &docName,
                                         const QStringList &alarmTexts,
                                         bool enabled)
{
    if (!m_enabled) return;

    if (alarmTexts.isEmpty()) {
        // У документа нет активных аварий — удаляем из очереди.
        m_docQueues.remove(docName);
        m_roundRobinOrder.removeAll(docName);

        // Если сейчас играет фраза этого документа — доигрываем её
        // (не обрываем), но новых фраз от него уже не будет.
        return;
    }

    auto it = m_docQueues.find(docName);
    if (it == m_docQueues.end()) {
        DocQueue q;
        q.texts = alarmTexts;
        q.nextIdx = 0;
        q.enabled = enabled;
        m_docQueues.insert(docName, q);
        m_roundRobinOrder.append(docName);
    } else {
        it->texts = alarmTexts;
        it->enabled = enabled;
        // nextIdx не сбрасываем — продолжаем с того же места,
        // чтобы при добавлении новой аварии не начинать сначала.
        if (it->nextIdx >= it->texts.size()) {
            it->nextIdx = 0;
        }
    }

    // Если сейчас ничего не играет — запускаем цикл.
    if (!m_playing && m_playingQueue.isEmpty()) {
        tryPlayNext();
    }
}

void AlarmSoundPlayer::unregisterDoc(const QString &docName)
{
    m_docQueues.remove(docName);
    m_roundRobinOrder.removeAll(docName);
}

void AlarmSoundPlayer::stopAll()
{
    m_docQueues.clear();
    m_roundRobinOrder.clear();
    m_playingQueue.clear();
    m_playing = false;
    if (m_player) {
        m_player->stop();
    }
}

// ============================================================================
// ВОСПРОИЗВЕДЕНИЕ
// ============================================================================

bool AlarmSoundPlayer::pickNextPhrase(QString &outDocName, QString &outAlarmText)
{
    if (m_docQueues.isEmpty()) return false;

    // Крутимся по m_roundRobinOrder, пока не найдём «активный» документ.
    // Каждый документ, который мы проверили, переносим в конец (round-robin).
    const int total = m_roundRobinOrder.size();
    for (int attempt = 0; attempt < total; ++attempt) {
        if (m_roundRobinOrder.isEmpty()) return false;

        const QString name = m_roundRobinOrder.takeFirst();
        m_roundRobinOrder.append(name);   // вернуть в конец

        auto it = m_docQueues.find(name);
        if (it == m_docQueues.end()) continue;
        if (!it->enabled) continue;
        if (it->texts.isEmpty()) continue;

        // Нашли.
        if (it->nextIdx >= it->texts.size()) it->nextIdx = 0;
        outDocName = name;
        outAlarmText = it->texts[it->nextIdx];
        it->nextIdx = (it->nextIdx + 1) % it->texts.size();
        return true;
    }

    return false;   // все документы выключены или пусты
}

void AlarmSoundPlayer::tryPlayNext()
{
    if (m_playing) return;
    if (!m_playingQueue.isEmpty()) return;

    QString docName;
    QString alarmText;
    if (!pickNextPhrase(docName, alarmText)) {
        return;   // нечего играть
    }

    m_currentDocName = docName;
    m_currentAlarmText = alarmText;

    // Формируем очередь файлов:
    //   1) <docName>.wav или default.wav
    //   2) <alarmText>.wav
    const QString docFile = filePathForText(docName);
    if (QFileInfo::exists(docFile)) {
        m_playingQueue.enqueue(docFile);
    } else {
        const QString defaultFile = filePathForText("default");
        if (QFileInfo::exists(defaultFile)) {
            m_playingQueue.enqueue(defaultFile);
        } else {
            qDebug() << "[AlarmSoundPlayer] Нет ни" << docFile
                     << "ни default.wav";
        }
    }

    const QString alarmFile = filePathForText(alarmText);
    if (QFileInfo::exists(alarmFile)) {
        m_playingQueue.enqueue(alarmFile);
    } else {
        qDebug() << "[AlarmSoundPlayer] Файл не найден:" << alarmFile;
    }

    if (m_playingQueue.isEmpty()) {
        // Ничего не нашли — сразу следующая фраза (через 0 мс, чтобы не зациклиться).
        QTimer::singleShot(0, this, [this]() { tryPlayNext(); });
        return;
    }

    playCurrentFile();
}

void AlarmSoundPlayer::playCurrentFile()
{
    if (m_playingQueue.isEmpty()) {
        // Фраза закончилась.
        m_playing = false;
        tryPlayNext();
        return;
    }

    const QString path = m_playingQueue.dequeue();
    m_playing = true;

    m_player->setMedia(QMediaContent(QUrl::fromLocalFile(path)));
    m_player->play();

    qDebug() << "[AlarmSoundPlayer] Играю:" << path;
}

void AlarmSoundPlayer::onMediaStateChanged(QMediaPlayer::State state)
{
    if (state == QMediaPlayer::StoppedState && m_playing) {
        m_playing = false;
        // Если в текущей фразе ещё есть файлы (например, осталось
        // <alarmText>.wav после <docName>.wav) — играем следующий.
        // Если фраза закончилась — playCurrentFile() сам вызовет tryPlayNext().
        playCurrentFile();
    }
}
