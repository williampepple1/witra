#ifndef TRANSFERMANAGER_H
#define TRANSFERMANAGER_H

#include <QObject>
#include <QMap>
#include "TransferItem.h"
#include "PeerManager.h"
#include "network/FileTransferServer.h"
#include "network/FileTransferClient.h"

namespace Witra {

class TransferManager : public QObject {
    Q_OBJECT
    
public:
    explicit TransferManager(PeerManager* peerManager, QObject* parent = nullptr);
    ~TransferManager();
    
    void start();
    void stop();
    bool isRunning() const { return m_running; }
    
    // Settings
    void setDownloadPath(const QString& path);
    QString downloadPath() const { return m_downloadPath; }
    
    // Transfers
    QList<TransferItem*> transfers() const { return m_transfers.values(); }
    TransferItem* transfer(const QString& id) const { return m_transfers.value(id, nullptr); }
    
    // Connection management
    void sendConnectionRequest(Peer* peer);
    void acceptConnectionRequest(TransferSession* session);
    void rejectConnectionRequest(TransferSession* session);
    
    // File operations
    void sendFiles(Peer* peer, const QStringList& filePaths);
    void sendFolder(Peer* peer, const QString& folderPath);
    void cancelTransfer(const QString& transferId);
    
signals:
    void connectionRequestReceived(TransferSession* session, const QString& senderName);
    void connectionAccepted(Peer* peer);
    void connectionRejected(Peer* peer);
    void transferAdded(TransferItem* transfer);
    void transferUpdated(TransferItem* transfer);
    void transferRemoved(const QString& transferId);
    void error(const QString& errorMessage);
    
private slots:
    void onConnectionRequestReceived(TransferSession* session, const QString& senderName);
    void onOutgoingConnectionReady(TransferSession* session);
    void onOutgoingConnectionFailed(const QString& error);
    void onSessionTransferStarted(const QString& transferId, const QString& fileName,
                                  qint64 totalSize, qint64 totalFiles);
    void onSessionTransferProgress(const QString& transferId, qint64 received, qint64 total);
    void onSessionTransferCompleted(const QString& transferId);
    void onSessionTransferFailed(const QString& transferId, const QString& error);
    
private:
    void setupSessionConnections(TransferSession* session);
    TransferSession* getOrCreateSession(Peer* peer);
    
    PeerManager* m_peerManager;
    FileTransferServer* m_server;
    FileTransferClient* m_client;
    QMap<QString, TransferItem*> m_transfers;
    QMap<QString, TransferSession*> m_pendingRequests; // peerId -> session
    QString m_downloadPath;
    bool m_running;
};

} // namespace Witra

#endif // TRANSFERMANAGER_H
