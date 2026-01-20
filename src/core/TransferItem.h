#ifndef TRANSFERITEM_H
#define TRANSFERITEM_H

#include <QObject>
#include <QString>
#include <QDateTime>

namespace Witra {

class TransferItem : public QObject {
    Q_OBJECT
    
public:
    enum class Direction {
        Incoming,
        Outgoing
    };
    Q_ENUM(Direction)
    
    enum class Status {
        Pending,
        InProgress,
        Completed,
        Failed,
        Cancelled
    };
    Q_ENUM(Status)
    
    explicit TransferItem(QObject* parent = nullptr);
    TransferItem(const QString& id, const QString& fileName, qint64 totalSize,
                 Direction direction, const QString& peerId, QObject* parent = nullptr);
    
    // Getters
    QString id() const { return m_id; }
    QString fileName() const { return m_fileName; }
    QString filePath() const { return m_filePath; }
    qint64 totalSize() const { return m_totalSize; }
    qint64 transferredSize() const { return m_transferredSize; }
    Direction direction() const { return m_direction; }
    Status status() const { return m_status; }
    QString peerId() const { return m_peerId; }
    QString peerName() const { return m_peerName; }
    QDateTime startTime() const { return m_startTime; }
    double progress() const;
    QString speedString() const;
    QString statusString() const;
    qint64 totalFiles() const { return m_totalFiles; }
    qint64 currentFile() const { return m_currentFile; }
    
    // Setters
    void setFilePath(const QString& path) { m_filePath = path; }
    void setPeerName(const QString& name) { m_peerName = name; }
    void setStatus(Status status);
    void setTransferredSize(qint64 size);
    void addTransferredBytes(qint64 bytes);
    void setTotalFiles(qint64 total) { m_totalFiles = total; }
    void setCurrentFile(qint64 current) { m_currentFile = current; }
    void setErrorMessage(const QString& error) { m_errorMessage = error; }
    
    QString errorMessage() const { return m_errorMessage; }
    
signals:
    void progressChanged(double progress);
    void statusChanged(Status status);
    void speedUpdated(qint64 bytesPerSecond);
    
private:
    void updateSpeed();
    
    QString m_id;
    QString m_fileName;
    QString m_filePath;
    qint64 m_totalSize;
    qint64 m_transferredSize;
    Direction m_direction;
    Status m_status;
    QString m_peerId;
    QString m_peerName;
    QDateTime m_startTime;
    qint64 m_totalFiles;
    qint64 m_currentFile;
    QString m_errorMessage;
    
    // Speed calculation
    qint64 m_lastSpeedBytes;
    QDateTime m_lastSpeedTime;
    qint64 m_currentSpeed;
};

} // namespace Witra

#endif // TRANSFERITEM_H
