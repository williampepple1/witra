#ifndef TRANSFERSESSION_H
#define TRANSFERSESSION_H

#include <QObject>
#include <QTcpSocket>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QFile>
#include <QDataStream>
#include <QCryptographicHash>
#include <QQueue>
#include <QTimer>
#include "Protocol.h"

namespace Witra {

class TransferSession : public QObject {
    Q_OBJECT
    
public:
    enum class State {
        Idle,
        WaitingForAccept,
        Accepted,
        Rejected,
        Transferring,
        Completed,
        Failed
    };
    Q_ENUM(State)
    
    explicit TransferSession(QTcpSocket* socket, QObject* parent = nullptr);
    ~TransferSession();
    
    void startServerEncryption(const QSslConfiguration& config);
    
    // Getters
    QString sessionId() const { return m_sessionId; }
    QString peerId() const { return m_peerId; }
    QString peerName() const { return m_peerName; }
    QHostAddress peerAddress() const;
    State state() const { return m_state; }
    bool isIncoming() const { return m_isIncoming; }
    QString verificationCode() const { return m_verificationCode; }
    qint64 maxFileSize() const { return m_maxFileSize; }
    
    // Setters
    void setPeerId(const QString& id) { m_peerId = id; }
    void setPeerName(const QString& name) { m_peerName = name; }
    void setIsIncoming(bool incoming) { m_isIncoming = incoming; }
    void setDownloadPath(const QString& path) { m_downloadPath = path; }
    void setMaxFileSize(qint64 size) { m_maxFileSize = size; }
    void setVerificationCode(const QString& code) { m_verificationCode = code; }
    
    // Connection requests
    void sendConnectionRequest(const QString& senderName, const QString& senderId);
    void sendConnectionAccept();
    void sendConnectionReject();
    
    // File transfer
    void sendFile(const QString& filePath, const QString& transferId, 
                  const QString& relativePath = QString(), 
                  qint64 totalFiles = 1, qint64 currentFile = 1);
    void sendFolder(const QString& folderPath, const QString& transferId);
    void cancelTransfer();
    
    // Socket
    QTcpSocket* socket() const { return m_socket; }
    void disconnectFromPeer();
    
signals:
    void connectionRequestReceived(const QString& senderName, const QString& senderId);
    void connectionAccepted();
    void connectionRejected();
    void transferStarted(const QString& transferId, const QString& fileName, 
                        qint64 totalSize, qint64 totalFiles);
    void transferProgress(const QString& transferId, qint64 bytesReceived, qint64 totalBytes);
    void fileReceived(const QString& transferId, const QString& filePath);
    void transferCompleted(const QString& transferId);
    void transferFailed(const QString& transferId, const QString& error);
    void disconnected();
    void error(const QString& errorMessage);
    
private slots:
    void onReadyRead();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void sendNextChunk();
    void sendNextQueuedFile();
    void onKeepaliveTimer();
    
private:
    void processMessage(const QByteArray& message);
    void handleConnectionRequest(const TransferHeader& header);
    void handleConnectionAccept(const TransferHeader& header);
    void handleConnectionReject(const TransferHeader& header);
    void handleFileHeader(const TransferHeader& header);
    void handleFileData(const QByteArray& data);
    void handleFileComplete(const TransferHeader& header);
    void handleTransferCancel(const TransferHeader& header);
    void handleFolderHeader(const TransferHeader& header);
    void handlePing(const TransferHeader& header);
    void handlePong();
    void sendPing();
    QString generateVerificationCode() const;
    
    void sendHeader(const TransferHeader& header);
    void writeMessage(const QByteArray& data, bool isHeader = true);
    
    QTcpSocket* m_socket;
    QString m_sessionId;
    QString m_peerId;
    QString m_peerName;
    bool m_isIncoming;
    State m_state;
    QString m_downloadPath;
    QString m_verificationCode;
    qint64 m_maxFileSize;
    
    // Message parsing
    QByteArray m_buffer;
    qint32 m_expectedSize;
    bool m_expectingHeader;
    
    // Current receiving file
    QString m_currentTransferId;
    QString m_currentFileName;
    QString m_currentRelativePath;
    QFile* m_currentFile;
    qint64 m_currentFileSize;
    qint64 m_currentBytesReceived;
    qint64 m_totalFiles;
    qint64 m_currentFileIndex;
    QByteArray m_expectedHash;
    QCryptographicHash m_runningHash{QCryptographicHash::Sha256};
    
    // Current sending file
    QFile* m_sendFile;
    QString m_sendTransferId;
    qint64 m_sendTotalSize;
    qint64 m_sendBytesSent;
    QCryptographicHash m_sendHash{QCryptographicHash::Sha256};
    
    struct QueuedFile {
        QString filePath;
        QString transferId;
        QString relativePath;
        qint64 totalFiles;
        qint64 currentFile;
    };
    QQueue<QueuedFile> m_sendQueue;
    
    // Folder send queue
    QStringList m_pendingSendFiles;
    QString m_pendingFolderTransferId;
    QString m_pendingFolderBasePath;
    qint64 m_pendingTotalFiles;
    qint64 m_pendingFileIndex;
    
    // Keepalive
    QTimer* m_keepaliveTimer;
    qint64 m_lastActivity;
};

} // namespace Witra

#endif // TRANSFERSESSION_H
