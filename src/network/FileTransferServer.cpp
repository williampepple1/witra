#include "FileTransferServer.h"
#include <QDir>

namespace Witra {

FileTransferServer::FileTransferServer(QObject* parent)
    : QObject(parent)
    , m_server(new CustomTcpServer(this))
    , m_maxFileSize(MAX_FILE_SIZE)
    , m_identity(nullptr)
{
    m_downloadPath = QDir::homePath() + "/Downloads/Witra";
    QDir().mkpath(m_downloadPath);

    connect(m_server, &CustomTcpServer::incomingSocketDescriptor,
            this, &FileTransferServer::handleIncomingConnection);
}

FileTransferServer::~FileTransferServer()
{
    stop();
}

void FileTransferServer::setTlsIdentity(TlsIdentity* identity)
{
    m_identity = identity;
    applyIdentity();
}

void FileTransferServer::applyIdentity()
{
    TlsIdentity* identity = m_identity ? m_identity : &TlsIdentity::application();
    if (!identity->ensure()) {
        m_sslConfig = QSslConfiguration();
        return;
    }
    m_sslConfig = identity->socketConfiguration();
}

bool FileTransferServer::start(quint16 port)
{
    if (m_server->isListening()) return true;

    applyIdentity();

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

void FileTransferServer::handleIncomingConnection(qintptr socketDescriptor)
{
    if (m_sessions.size() >= MAX_CONNECTIONS) {
        QTcpSocket rejected;
        rejected.setSocketDescriptor(socketDescriptor);
        rejected.disconnectFromHost();
        emit error(tr("Connection rejected: maximum connections (%1) reached").arg(MAX_CONNECTIONS));
        return;
    }

    QTcpSocket* sessionSocket = nullptr;

    if (QSslSocket::supportsSsl() && !m_sslConfig.localCertificate().isNull()) {
        QSslSocket* sslSocket = new QSslSocket(this);
        if (!sslSocket->setSocketDescriptor(socketDescriptor)) {
            emit error(tr("Failed to attach SSL to incoming connection"));
            delete sslSocket;
            return;
        }
        sslSocket->setSslConfiguration(m_sslConfig);
        connect(sslSocket, &QSslSocket::sslErrors, this, [this, sslSocket](const QList<QSslError>& errors) {
            QString reason;
            if (TlsIdentity::evaluateHandshake(sslSocket, QString(), errors, &reason)) {
                sslSocket->ignoreSslErrors();
            } else {
                emit error(tr("Rejected unauthenticated TLS peer: %1").arg(reason));
                sslSocket->abort();
            }
        });
        sslSocket->startServerEncryption();
        sessionSocket = sslSocket;
    } else {
        sessionSocket = new QTcpSocket(this);
        if (!sessionSocket->setSocketDescriptor(socketDescriptor)) {
            emit error(tr("Failed to attach to incoming connection"));
            delete sessionSocket;
            return;
        }
    }

    TransferSession* session = new TransferSession(sessionSocket, this);
    session->setIsIncoming(true);
    session->setDownloadPath(m_downloadPath);
    session->setMaxFileSize(m_maxFileSize);

    m_sessions[session->sessionId()] = session;

    connect(session, &TransferSession::disconnected,
            this, &FileTransferServer::onSessionDisconnected);

    connect(session, &TransferSession::connectionRequestReceived,
            this, [this, session](const QString& senderName, const QString&) {
        emit connectionRequestReceived(session, senderName);
    });

    emit newConnection(session);
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
