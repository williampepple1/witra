#include "TransferManager.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace Witra {

TransferManager::TransferManager(PeerManager* peerManager, QObject* parent)
    : QObject(parent)
    , m_peerManager(peerManager)
    , m_server(new FileTransferServer(this))
    , m_client(new FileTransferClient(this))
    , m_running(false)
{
    // Load download path from settings (set by installer or user)
    QSettings settings;
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Witra Downloads";
    m_downloadPath = settings.value("DownloadPath", defaultPath).toString();
    
    // Fallback if path is empty
    if (m_downloadPath.isEmpty()) {
        m_downloadPath = QDir::homePath() + "/Downloads/Witra";
    }
    
    QDir().mkpath(m_downloadPath);
    
    // Server signals
    connect(m_server, &FileTransferServer::connectionRequestReceived,
            this, &TransferManager::onConnectionRequestReceived);
    connect(m_server, &FileTransferServer::error,
            this, &TransferManager::error);
    connect(m_server, &FileTransferServer::newConnection,
            this, [this](TransferSession* session) {
        connect(session, &TransferSession::disconnected,
                this, [this, session]() { onSessionDisconnected(session); });
    });
    
    // Client signals
    connect(m_client, &FileTransferClient::connected,
            this, &TransferManager::onOutgoingConnectionReady);
    connect(m_client, &FileTransferClient::connectionFailed,
            this, &TransferManager::onOutgoingConnectionFailed);
}

TransferManager::~TransferManager()
{
    stop();
}

void TransferManager::start()
{
    if (m_running) return;
    
    m_server->setDownloadPath(m_downloadPath);
    m_client->setDownloadPath(m_downloadPath);
    
    if (!m_server->start()) {
        return;
    }
    
    m_running = true;
}

void TransferManager::stop()
{
    if (!m_running) return;
    
    m_server->stop();
    m_running = false;
}

void TransferManager::setDownloadPath(const QString& path)
{
    m_downloadPath = path;
    QDir().mkpath(m_downloadPath);
    m_server->setDownloadPath(path);
    m_client->setDownloadPath(path);
}

void TransferManager::sendConnectionRequest(Peer* peer)
{
    if (!peer || peer->state() != Peer::ConnectionState::Discovered) return;
    
    TransferSession* session = m_client->connectToPeer(peer->address(), peer->port());
    if (session) {
        session->setPeerId(peer->id());
        session->setPeerName(peer->displayName());
        m_pendingRequests[peer->id()] = session;
        peer->setState(Peer::ConnectionState::RequestSent);
    }
}

void TransferManager::acceptConnectionRequest(TransferSession* session)
{
    if (!session) return;
    
    session->sendConnectionAccept();
    
    // Find and update peer
    Peer* peer = m_peerManager->peer(session->peerId());
    if (peer) {
        peer->setState(Peer::ConnectionState::Connected);
        emit connectionAccepted(peer);
    }
    
    setupSessionConnections(session);
}

void TransferManager::rejectConnectionRequest(TransferSession* session)
{
    if (!session) return;
    
    session->sendConnectionReject();
    
    // Find and update peer
    Peer* peer = m_peerManager->peer(session->peerId());
    if (peer) {
        peer->setState(Peer::ConnectionState::Discovered);
        emit connectionRejected(peer);
    }
}

void TransferManager::disconnectFromPeer(Peer* peer)
{
    if (!peer) return;
    
    // Check for active transfers first
    if (hasActiveTransfersWithPeer(peer->id())) {
        emit error(tr("Cannot disconnect while transfers are in progress"));
        return;
    }
    
    // Find and close any sessions with this peer
    QString peerId = peer->id();
    
    // Close client sessions
    TransferSession* clientSession = m_client->sessionByPeerId(peerId);
    if (clientSession) {
        clientSession->disconnectFromPeer();
    }
    
    // Close server sessions
    for (TransferSession* session : m_server->sessions()) {
        if (session->peerId() == peerId) {
            session->disconnectFromPeer();
        }
    }
    
    // Remove from pending requests
    m_pendingRequests.remove(peerId);
    
    // Update peer state
    peer->setState(Peer::ConnectionState::Discovered);
}

bool TransferManager::hasActiveTransfersWithPeer(const QString& peerId) const
{
    for (TransferItem* item : m_transfers.values()) {
        if (item->peerId() == peerId) {
            TransferItem::Status status = item->status();
            if (status == TransferItem::Status::Pending || 
                status == TransferItem::Status::InProgress) {
                return true;
            }
        }
    }
    return false;
}

void TransferManager::sendFiles(Peer* peer, const QStringList& filePaths)
{
    if (!peer || !peer->isConnected()) return;
    
    TransferSession* session = getOrCreateSession(peer);
    if (!session) return;
    
    for (int i = 0; i < filePaths.size(); ++i) {
        const QString& filePath = filePaths[i];
        QFileInfo fileInfo(filePath);
        
        if (!fileInfo.exists()) continue;
        
        if (fileInfo.isDir()) {
            // Send as folder
            sendFolder(peer, filePath);
        } else {
            // Send as file
            QString transferId = generateUniqueId();
            
            TransferItem* item = new TransferItem(
                transferId, fileInfo.fileName(), fileInfo.size(),
                TransferItem::Direction::Outgoing, peer->id(), this
            );
            item->setFilePath(filePath);
            item->setPeerName(peer->displayName());
            item->setStatus(TransferItem::Status::InProgress);
            
            m_transfers[transferId] = item;
            emit transferAdded(item);
            
            session->sendFile(filePath, transferId, QString(), 
                             filePaths.size(), i + 1);
        }
    }
}

void TransferManager::sendFolder(Peer* peer, const QString& folderPath)
{
    if (!peer || !peer->isConnected()) return;
    
    TransferSession* session = getOrCreateSession(peer);
    if (!session) return;
    
    QDir dir(folderPath);
    if (!dir.exists()) return;
    
    QString transferId = generateUniqueId();
    
    // Calculate total size
    qint64 totalSize = 0;
    qint64 fileCount = 0;
    QDirIterator it(folderPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        totalSize += it.fileInfo().size();
        fileCount++;
    }
    
    TransferItem* item = new TransferItem(
        transferId, dir.dirName(), totalSize,
        TransferItem::Direction::Outgoing, peer->id(), this
    );
    item->setFilePath(folderPath);
    item->setPeerName(peer->displayName());
    item->setTotalFiles(fileCount);
    item->setStatus(TransferItem::Status::InProgress);
    
    m_transfers[transferId] = item;
    emit transferAdded(item);
    
    session->sendFolder(folderPath, transferId);
}

void TransferManager::cancelTransfer(const QString& transferId)
{
    TransferItem* item = m_transfers.value(transferId, nullptr);
    if (!item) return;
    
    item->setStatus(TransferItem::Status::Cancelled);
    emit transferUpdated(item);
    
    // Find session and cancel
    for (TransferSession* session : m_server->sessions()) {
        session->cancelTransfer();
    }
    for (TransferSession* session : m_client->sessions()) {
        session->cancelTransfer();
    }
}

void TransferManager::onConnectionRequestReceived(TransferSession* session, 
                                                   const QString& senderName)
{
    // Find peer by address or create temporary reference
    QString peerId = session->peerId();
    Peer* peer = m_peerManager->peer(peerId);
    
    if (peer) {
        peer->setState(Peer::ConnectionState::RequestReceived);
    }
    
    emit connectionRequestReceived(session, senderName);
}

void TransferManager::onOutgoingConnectionReady(TransferSession* session)
{
    // Send connection request
    session->sendConnectionRequest(m_peerManager->displayName(), m_peerManager->peerId());
    
    connect(session, &TransferSession::connectionAccepted, this, [this, session]() {
        Peer* peer = m_peerManager->peer(session->peerId());
        if (peer) {
            peer->setState(Peer::ConnectionState::Connected);
            emit connectionAccepted(peer);
        }
        setupSessionConnections(session);
    });
    
    connect(session, &TransferSession::connectionRejected, this, [this, session]() {
        Peer* peer = m_peerManager->peer(session->peerId());
        if (peer) {
            peer->setState(Peer::ConnectionState::Discovered);
            emit connectionRejected(peer);
        }
        m_pendingRequests.remove(session->peerId());
    });
}

void TransferManager::onOutgoingConnectionFailed(const QString& error)
{
    emit this->error(tr("Connection failed: %1").arg(error));
}

void TransferManager::setupSessionConnections(TransferSession* session)
{
    connect(session, &TransferSession::transferStarted,
            this, &TransferManager::onSessionTransferStarted);
    connect(session, &TransferSession::transferProgress,
            this, &TransferManager::onSessionTransferProgress);
    connect(session, &TransferSession::transferCompleted,
            this, &TransferManager::onSessionTransferCompleted);
    connect(session, &TransferSession::transferFailed,
            this, &TransferManager::onSessionTransferFailed);
    connect(session, &TransferSession::disconnected,
            this, [this, session]() { onSessionDisconnected(session); });
}

void TransferManager::onSessionTransferStarted(const QString& transferId, 
                                                const QString& fileName,
                                                qint64 totalSize, qint64 totalFiles)
{
    TransferSession* session = qobject_cast<TransferSession*>(sender());
    if (!session) return;
    
    TransferItem* item = new TransferItem(
        transferId, fileName, totalSize,
        TransferItem::Direction::Incoming, session->peerId(), this
    );
    item->setPeerName(session->peerName());
    item->setTotalFiles(totalFiles);
    item->setStatus(TransferItem::Status::InProgress);
    
    m_transfers[transferId] = item;
    emit transferAdded(item);
}

void TransferManager::onSessionTransferProgress(const QString& transferId,
                                                 qint64 received, qint64 total)
{
    Q_UNUSED(total)
    TransferItem* item = m_transfers.value(transferId, nullptr);
    if (item) {
        item->setTransferredSize(received);
        emit transferUpdated(item);
    }
}

void TransferManager::onSessionTransferCompleted(const QString& transferId)
{
    TransferItem* item = m_transfers.value(transferId, nullptr);
    if (item) {
        item->setStatus(TransferItem::Status::Completed);
        emit transferUpdated(item);
    }
}

void TransferManager::onSessionTransferFailed(const QString& transferId, 
                                               const QString& error)
{
    TransferItem* item = m_transfers.value(transferId, nullptr);
    if (item) {
        item->setStatus(TransferItem::Status::Failed);
        item->setErrorMessage(error);
        emit transferUpdated(item);
    }
}

TransferSession* TransferManager::getOrCreateSession(Peer* peer)
{
    // Check if we already have a session
    TransferSession* session = m_client->sessionByPeerId(peer->id());
    if (session) return session;
    
    // Check server sessions
    for (TransferSession* s : m_server->sessions()) {
        if (s->peerId() == peer->id()) {
            return s;
        }
    }
    
    // Create new connection
    return m_client->connectToPeer(peer->address(), peer->port());
}

void TransferManager::onSessionDisconnected(TransferSession* session)
{
    if (!session) return;
    
    QString peerId = session->peerId();
    if (!peerId.isEmpty()) {
        updatePeerStateOnDisconnect(peerId);
    }
    
    // Remove from pending requests
    m_pendingRequests.remove(peerId);
    
    // Mark any active transfers with this peer as failed
    for (TransferItem* item : m_transfers.values()) {
        if (item->peerId() == peerId) {
            if (item->status() == TransferItem::Status::InProgress ||
                item->status() == TransferItem::Status::Pending) {
                item->setStatus(TransferItem::Status::Failed);
                item->setErrorMessage(tr("Connection lost"));
                emit transferUpdated(item);
            }
        }
    }
}

void TransferManager::updatePeerStateOnDisconnect(const QString& peerId)
{
    Peer* peer = m_peerManager->peer(peerId);
    if (!peer) return;
    
    // Only update if currently connected or in a connection process
    if (peer->state() == Peer::ConnectionState::Connected ||
        peer->state() == Peer::ConnectionState::RequestSent ||
        peer->state() == Peer::ConnectionState::RequestReceived) {
        
        // Check if there are any remaining sessions with this peer
        bool hasOtherSessions = false;
        
        for (TransferSession* s : m_client->sessions()) {
            if (s->peerId() == peerId) {
                hasOtherSessions = true;
                break;
            }
        }
        
        if (!hasOtherSessions) {
            for (TransferSession* s : m_server->sessions()) {
                if (s->peerId() == peerId) {
                    hasOtherSessions = true;
                    break;
                }
            }
        }
        
        // If no more sessions, revert to Discovered state
        if (!hasOtherSessions) {
            peer->setState(Peer::ConnectionState::Discovered);
        }
    }
}

} // namespace Witra
