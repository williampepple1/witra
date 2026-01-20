#include "TransferItem.h"
#include <QLocale>

namespace Witra {

TransferItem::TransferItem(QObject* parent)
    : QObject(parent)
    , m_totalSize(0)
    , m_transferredSize(0)
    , m_direction(Direction::Incoming)
    , m_status(Status::Pending)
    , m_startTime(QDateTime::currentDateTime())
    , m_totalFiles(1)
    , m_currentFile(1)
    , m_lastSpeedBytes(0)
    , m_lastSpeedTime(QDateTime::currentDateTime())
    , m_currentSpeed(0)
{
}

TransferItem::TransferItem(const QString& id, const QString& fileName, qint64 totalSize,
                           Direction direction, const QString& peerId, QObject* parent)
    : QObject(parent)
    , m_id(id)
    , m_fileName(fileName)
    , m_totalSize(totalSize)
    , m_transferredSize(0)
    , m_direction(direction)
    , m_status(Status::Pending)
    , m_peerId(peerId)
    , m_startTime(QDateTime::currentDateTime())
    , m_totalFiles(1)
    , m_currentFile(1)
    , m_lastSpeedBytes(0)
    , m_lastSpeedTime(QDateTime::currentDateTime())
    , m_currentSpeed(0)
{
}

double TransferItem::progress() const
{
    if (m_totalSize == 0) return 0.0;
    return static_cast<double>(m_transferredSize) / static_cast<double>(m_totalSize) * 100.0;
}

QString TransferItem::speedString() const
{
    if (m_currentSpeed == 0) return QString();
    
    QLocale locale;
    if (m_currentSpeed >= 1024 * 1024 * 1024) {
        return QString("%1 GB/s").arg(locale.toString(m_currentSpeed / (1024.0 * 1024.0 * 1024.0), 'f', 2));
    } else if (m_currentSpeed >= 1024 * 1024) {
        return QString("%1 MB/s").arg(locale.toString(m_currentSpeed / (1024.0 * 1024.0), 'f', 2));
    } else if (m_currentSpeed >= 1024) {
        return QString("%1 KB/s").arg(locale.toString(m_currentSpeed / 1024.0, 'f', 2));
    } else {
        return QString("%1 B/s").arg(locale.toString(m_currentSpeed));
    }
}

QString TransferItem::statusString() const
{
    switch (m_status) {
        case Status::Pending: return tr("Waiting");
        case Status::InProgress: return tr("Transferring");
        case Status::Completed: return tr("Completed");
        case Status::Failed: return tr("Failed");
        case Status::Cancelled: return tr("Cancelled");
        default: return tr("Unknown");
    }
}

void TransferItem::setStatus(Status status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged(status);
    }
}

void TransferItem::setTransferredSize(qint64 size)
{
    m_transferredSize = size;
    updateSpeed();
    emit progressChanged(progress());
}

void TransferItem::addTransferredBytes(qint64 bytes)
{
    m_transferredSize += bytes;
    updateSpeed();
    emit progressChanged(progress());
}

void TransferItem::updateSpeed()
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 elapsed = m_lastSpeedTime.msecsTo(now);
    
    if (elapsed >= 1000) { // Update speed every second
        qint64 bytesDiff = m_transferredSize - m_lastSpeedBytes;
        m_currentSpeed = bytesDiff * 1000 / elapsed;
        m_lastSpeedBytes = m_transferredSize;
        m_lastSpeedTime = now;
        emit speedUpdated(m_currentSpeed);
    }
}

} // namespace Witra
