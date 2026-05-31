#include "FileTransferClient.h"
#include <QDir>

namespace Witra {

FileTransferClient::FileTransferClient(QObject* parent)
    : QObject(parent)
    , m_maxFileSize(MAX_FILE_SIZE)
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
    QTcpSocket* socket;
    
    if (QSslSocket::supportsSsl()) {
        QSslSocket* ssl = new QSslSocket(this);
        ssl->setPeerVerifyMode(QSslSocket::VerifyNone);
        ssl->setProtocol(QSsl::TlsV1_2OrLater);
        connect(ssl, &QSslSocket::sslErrors, ssl, [ssl](const QList<QSslError>&) {
            ssl->ignoreSslErrors();
        });
        socket = ssl;
    } else {
        socket = new QTcpSocket(this);
    }
    
    TransferSession* session = new TransferSession(socket, this);
    session->setIsIncoming(false);
    session->setDownloadPath(m_downloadPath);
    session->setMaxFileSize(m_maxFileSize);
    
    m_sessions[session->sessionId()] = session;
    
    connect(session, &TransferSession::disconnected,
            this, &FileTransferClient::onSessionDisconnected);
    
    QSslSocket* sslSocket = qobject_cast<QSslSocket*>(socket);
    if (sslSocket) {
        bool* downgraded = new bool(false);
        connect(sslSocket, &QSslSocket::encrypted, this, [this, session, downgraded]() {
            *downgraded = true;
            emit connected(session);
        });
        connect(sslSocket, &QSslSocket::errorOccurred, this,
                [this, session, downgraded, address, port](QAbstractSocket::SocketError) mutable {
            if (*downgraded) return;
            *downgraded = true;
            session->deleteLater();
            m_sessions.remove(session->sessionId());
            
            QTcpSocket* plain = new QTcpSocket(this);
            TransferSession* retry = new TransferSession(plain, this);
            retry->setIsIncoming(false);
            retry->setDownloadPath(m_downloadPath);
            retry->setMaxFileSize(m_maxFileSize);
            m_sessions[retry->sessionId()] = retry;
            connect(retry, &TransferSession::disconnected, this, &FileTransferClient::onSessionDisconnected);
            connect(plain, &QTcpSocket::connected, this, [this, retry]() {
                emit connected(retry);
            });
            connect(plain, &QTcpSocket::errorOccurred, this,
                    [this, retry](QAbstractSocket::SocketError) {
                emit connectionFailed(retry, retry->socket()->errorString());
                m_sessions.remove(retry->sessionId());
                retry->deleteLater();
            });
            plain->connectToHost(address.toString(), port);
        });
        connect(sslSocket, &QSslSocket::errorOccurred, this,
                [this, session](QAbstractSocket::SocketError) {
            emit connectionFailed(session, session->socket()->errorString());
            m_sessions.remove(session->sessionId());
            session->deleteLater();
        });
        sslSocket->connectToHostEncrypted(address.toString(), port);
    } else {
        connect(socket, &QTcpSocket::connected, this, [this, session]() {
            emit connected(session);
        });
        connect(socket, &QTcpSocket::errorOccurred, this,
                [this, session](QAbstractSocket::SocketError) {
            emit connectionFailed(session, session->socket()->errorString());
            m_sessions.remove(session->sessionId());
            session->deleteLater();
        });
        socket->connectToHost(address.toString(), port);
    }
    
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
