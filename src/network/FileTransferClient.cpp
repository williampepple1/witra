#include "FileTransferClient.h"
#include <QDir>

namespace Witra {

FileTransferClient::FileTransferClient(QObject* parent)
    : QObject(parent)
{
    m_downloadPath = QDir::homePath() + "/Downloads/Witra";
}

FileTransferClient::~FileTransferClient()
{
    for (TransferSession* session : m_sessions.values()) {
        session->disconnectFromPeer();
        session->deleteLater();
    }
    m_sessions.clear();
}

TransferSession* FileTransferClient::connectToPeer(const QHostAddress& address, quint16 port)
{
    QTcpSocket* socket = new QTcpSocket(this);
    
    TransferSession* session = new TransferSession(socket, this);
    session->setIsIncoming(false);
    session->setDownloadPath(m_downloadPath);
    
    m_sessions[session->sessionId()] = session;
    
    connect(session, &TransferSession::disconnected,
            this, &FileTransferClient::onSessionDisconnected);
    
    connect(socket, &QTcpSocket::connected, this, [this, session]() {
        emit connected(session);
    });
    
    connect(socket, &QTcpSocket::errorOccurred, this, 
            [this, session](QAbstractSocket::SocketError) {
        emit connectionFailed(session->socket()->errorString());
        m_sessions.remove(session->sessionId());
        session->deleteLater();
    });
    
    socket->connectToHost(address, port);
    
    return session;
}

TransferSession* FileTransferClient::session(const QString& sessionId) const
{
    return m_sessions.value(sessionId, nullptr);
}

TransferSession* FileTransferClient::sessionByPeerId(const QString& peerId) const
{
    for (TransferSession* session : m_sessions.values()) {
        if (session->peerId() == peerId) {
            return session;
        }
    }
    return nullptr;
}

void FileTransferClient::onSessionDisconnected()
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
