#include "FileTransferServer.h"
#include <QDir>
#include <QFile>
#include <QSslCertificate>
#include <QSslKey>

namespace Witra {

FileTransferServer::FileTransferServer(QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_connectionCount(0)
    , m_maxFileSize(MAX_FILE_SIZE)
{
    QFile certFile(":/certs/cert.pem");
    QFile keyFile(":/certs/key.pem");
    
    if (certFile.open(QIODevice::ReadOnly) && keyFile.open(QIODevice::ReadOnly)) {
        QSslCertificate cert(&certFile, QSsl::Pem);
        QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);
        
        if (!cert.isNull() && !key.isNull()) {
            m_sslConfig.setLocalCertificate(cert);
            m_sslConfig.setPrivateKey(key);
            m_sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
            m_sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
        }
    }
    
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
    
    for (int offset = 0; offset < MAX_PORT_RANGE; ++offset) {
        quint16 tryPort = port + offset;
        if (m_server->listen(QHostAddress::AnyIPv4, tryPort)) {
            return true;
        }
    }
    
    emit error(tr("Failed to start server on ports %1-%2: %3")
               .arg(port).arg(port + MAX_PORT_RANGE - 1)
               .arg(m_server->errorString()));
    return false;
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
        if (m_connectionCount >= MAX_CONNECTIONS) {
            QTcpSocket* rejected = m_server->nextPendingConnection();
            rejected->disconnectFromHost();
            rejected->deleteLater();
            continue;
        }
        
        QTcpSocket* socket = m_server->nextPendingConnection();
        
        QSslSocket* sslSocket = new QSslSocket(this);
        sslSocket->setSocketDescriptor(socket->socketDescriptor());
        socket->deleteLater();
        
        sslSocket->setSslConfiguration(m_sslConfig);
        sslSocket->startServerEncryption();
        
        TransferSession* session = new TransferSession(sslSocket, this);
        session->setIsIncoming(true);
        session->setDownloadPath(m_downloadPath);
        session->setMaxFileSize(m_maxFileSize);
        
        m_sessions[session->sessionId()] = session;
        m_connectionCount++;
        
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
        m_connectionCount--;
        emit sessionClosed(sessionId);
        session->deleteLater();
    }
}

} // namespace Witra
