#include "FileTransferServer.h"
#include <QDir>

namespace Witra {

FileTransferServer::FileTransferServer(QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    // Default download path
    m_downloadPath = QDir::homePath() + "/Downloads/Witra";
    QDir().mkpath(m_downloadPath);
    
    connect(m_server, &QTcpServer::newConnection, 
            this, &FileTransferServer::onNewConnection);
}

FileTransferServer::~FileTransferServer()
{
    stop();
}

bool FileTransferServer::start(quint16 port)
{
    if (m_server->isListening()) return true;
    
    if (!m_server->listen(QHostAddress::AnyIPv4, port)) {
        emit error(tr("Failed to start server on port %1: %2")
                   .arg(port).arg(m_server->errorString()));
        return false;
    }
    
    return true;
}

void FileTransferServer::stop()
{
    m_server->close();
    
    for (TransferSession* session : m_sessions.values()) {
        session->disconnectFromPeer();
        session->deleteLater();
    }
    m_sessions.clear();
}

bool FileTransferServer::isListening() const
{
    return m_server->isListening();
}

quint16 FileTransferServer::port() const
{
    return m_server->serverPort();
}

TransferSession* FileTransferServer::session(const QString& sessionId) const
{
    return m_sessions.value(sessionId, nullptr);
}

void FileTransferServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        
        TransferSession* session = new TransferSession(socket, this);
        session->setIsIncoming(true);
        session->setDownloadPath(m_downloadPath);
        
        m_sessions[session->sessionId()] = session;
        
        connect(session, &TransferSession::disconnected,
                this, &FileTransferServer::onSessionDisconnected);
        
        connect(session, &TransferSession::connectionRequestReceived,
                this, [this, session](const QString& senderName, const QString&) {
            emit connectionRequestReceived(session, senderName);
        });
        
        emit newConnection(session);
    }
}

void FileTransferServer::onSessionDisconnected()
{
    TransferSession* session = qobject_cast<TransferSession*>(sender());
    if (session) {
        QString sessionId = session->sessionId();
        m_sessions.remove(sessionId);
        emit sessionClosed(sessionId);
        session->deleteLater();
    }
}

} // namespace Witra
