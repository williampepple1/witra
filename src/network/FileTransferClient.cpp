#include "FileTransferClient.h"
#include <QDir>

namespace Witra {

FileTransferClient::FileTransferClient(QObject* parent)
    : QObject(parent)
    , m_maxFileSize(MAX_FILE_SIZE)
    , m_identity(nullptr)
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

void FileTransferClient::setTlsIdentity(TlsIdentity* identity)
{
    m_identity = identity;
}

TlsIdentity* FileTransferClient::identity() const
{
    return m_identity ? m_identity : &TlsIdentity::application();
}

TransferSession* FileTransferClient::connectToPeer(const QHostAddress& address, quint16 port,
                                                   const QString& expectedPeerId)
{
    QTcpSocket* socket = nullptr;
    TlsIdentity* tls = identity();
    const bool useTls = QSslSocket::supportsSsl() && tls->ensure();

    if (useTls) {
        QSslSocket* ssl = new QSslSocket(this);
        ssl->setSslConfiguration(tls->socketConfiguration());
        socket = ssl;
    } else {
        socket = new QTcpSocket(this);
    }

    TransferSession* session = new TransferSession(socket, this);
    session->setIsIncoming(false);
    session->setDownloadPath(m_downloadPath);
    session->setMaxFileSize(m_maxFileSize);
    if (!expectedPeerId.isEmpty()) {
        session->setPeerId(expectedPeerId);
    }

    m_sessions[session->sessionId()] = session;

    connect(session, &TransferSession::disconnected,
            this, &FileTransferClient::onSessionDisconnected);

    QSslSocket* sslSocket = qobject_cast<QSslSocket*>(socket);
    if (sslSocket) {
        connect(sslSocket, &QSslSocket::sslErrors, this,
                [this, sslSocket, session, expectedPeerId](const QList<QSslError>& errors) {
            QString reason;
            if (TlsIdentity::evaluateHandshake(sslSocket, expectedPeerId, errors, &reason)) {
                sslSocket->ignoreSslErrors();
            } else {
                emit connectionFailed(session, reason);
                sslSocket->abort();
            }
        });
        connect(sslSocket, &QSslSocket::encrypted, this, [this, session]() {
            emit connected(session);
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
        socket->connectToHost(address, port);
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
