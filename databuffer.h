#ifndef DATABUFFER_H
#define DATABUFFER_H

#include <QMutex>
#include <QMutexLocker>
#include <functional>

template <typename T>
class DoubleBuffer
{
public:
    DoubleBuffer() : m_activeWriteBuffer(0) {}

    // ✅ Вызывается из потока ДРАЙВЕРА: обновляет данные
    void update(const std::function<void(T&)>& updateFunc)
    {
        QMutexLocker locker(&m_mutex);
        updateFunc(m_buffers[m_activeWriteBuffer]);
    }

    // ✅ НОВОЕ: Безопасное чтение последних данных БЕЗ переключения буферов
    // Идеально подходит для таймера UI, чтобы не "съедать" данные
    T getLatest() const
    {
        QMutexLocker locker(&m_mutex);
        return m_buffers[m_activeWriteBuffer];
    }

    // ✅ Вызывается, если нужно именно забрать данные и очистить буфер (для логирования)
    T swapAndGet()
    {
        QMutexLocker locker(&m_mutex);
        T result = m_buffers[m_activeWriteBuffer];
        m_activeWriteBuffer = 1 - m_activeWriteBuffer;
        m_buffers[m_activeWriteBuffer] = T(); // Очистка нового буфера записи
        return result;
    }

private:
    T m_buffers[2];
    int m_activeWriteBuffer;
    mutable QMutex m_mutex;
};

#endif // DATABUFFER_H
