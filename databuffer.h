#ifndef DATABUFFER_H
#define DATABUFFER_H

#include <QMutex>
#include <QMutexLocker>
#include <functional>

// ============================================================================
// ✅ Универсальный класс двойной буферизации (Thread-Safe Double Buffer)
// Позволяет одному потоку (драйверу) постоянно писать данные,
// а другому потоку (логгеру/UI) читать и забирать их без блокировок.
// ============================================================================
template <typename T>
class DoubleBuffer
{
public:
    DoubleBuffer() : m_activeWriteBuffer(0) {}

    // ✅ Вызывается из потока ДРАЙВЕРА: обновляет данные в текущем буфере записи
    void update(const std::function<void(T&)>& updateFunc)
    {
        QMutexLocker locker(&m_mutex);
        updateFunc(m_buffers[m_activeWriteBuffer]);
    }

    // ✅ Вызывается из потока ЛОГГЕРА/UI: забирает актуальные данные и переключает буфер
    T swapAndGet()
    {
        QMutexLocker locker(&m_mutex);
        // Забираем данные из текущего буфера записи
        T result = m_buffers[m_activeWriteBuffer];

        // Переключаем активный буфер для записи (0 -> 1 или 1 -> 0)
        m_activeWriteBuffer = 1 - m_activeWriteBuffer;

        // Очищаем новый буфер записи (опционально, чтобы не копировать старые данные)
        m_buffers[m_activeWriteBuffer] = T();

        return result;
    }

private:
    T m_buffers[2];          // Два буфера: один читается, другой пишется
    int m_activeWriteBuffer; // Индекс буфера, в который сейчас пишет драйвер
    mutable QMutex m_mutex;  // Мьютекс защищает только момент swap и update
};

#endif // DATABUFFER_H
