#include "pollchain.h"
#include "pollscheduler.h"   // ✅ Здесь PollScheduler полностью определён
#include <QDateTime>

void PollChain::notifyTimePool(const QDateTime &time)
{
    if (m_scheduler) {
        m_scheduler->setTimePoolTime(time);
    }
}
