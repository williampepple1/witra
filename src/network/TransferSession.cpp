#include "TransferSession.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QTimer>
#include <QRandomGenerator>

namespace Witra {

TransferSession::TransferSession(QTcpSocket* socket, QObject* parent)
    : QObject(parent)
    , m_socket(socket)
    , m_sessionId(generateUniqueId())
    , m_isIncoming(false)
    , m_state(State::Idle)
    , m_expectedSize(0)
    , m_expectingHeader(true)
    , m_currentFile(nullptr)
    , m_currentFileSize(0)
    , m_currentBytesReceived(0)
    , m_totalFiles(0)
    , m_currentFileIndex(0)
    , m_sendFile(nullptr)
    , m_sendTotalSize(0)
    , m_sendBytesSent(0)
    , m_maxFileSize(MAX_FILE_SIZE)
    , m_runningHash(QCryptographicHash::Sha256)
{
    if (m_socket) {
        m_socket->setParent(this);
        connect(m_socket, &QTcpSocket::readyRead, this, &TransferSession::onReadyRead);
        connect(m_socket, &QTcpSocket::disconnected, this, &TransferSession::onDisconnected);
        connect(m_socket, &QTcpSocket::errorOccurred, this, &TransferSession::onSocketError);
    }
}

TransferSession::~TransferSession()
{
    if (m_currentFile) {
        m_currentFile->close();
        delete m_currentFile;
    }
    if (m_sendFile) {
        m_sendFile->close();
        delete m_sendFile;
    }
}

QHostAddress TransferSession::peerAddress() const
{
    return m_socket ? m_socket->peerAddress() : QHostAddress();
}

void TransferSession::sendConnectionRequest(const QString& senderName, const QString& senderId)
{
    m_verificationCode = generateVerificationCode();
    
    TransferHeader header;
    header.type = TransferType::CONNECTION_REQUEST;
    header.senderName = senderName;
    header.transferId = senderId;
    header.verificationCode = m_verificationCode;
    
    sendHeader(header);
    m_state = State::WaitingForAccept;
}

void TransferSession::sendConnectionAccept()
{
    TransferHeader header;
    header.type = TransferType::CONNECTION_ACCEPT;
    
    sendHeader(header);
    m_state = State::Accepted;
}

void TransferSession::sendConnectionReject()
{
    TransferHeader header;
    header.type = TransferType::CONNECTION_REJECT;
    
    sendHeader(header);
    m_state = State::Rejected;
}

void TransferSession::sendFile(const QString& filePath, const QString& transferId,
                               const QString& relativePath, qint64 totalFiles, qint64 currentFile)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        emit transferFailed(transferId, tr("File not found: %1").arg(filePath));
        return;
    }
    
    if (m_sendFile) {
        return;
    }
    
    m_sendFile = new QFile(filePath, this);
    if (!m_sendFile->open(QIODevice::ReadOnly)) {
        emit transferFailed(transferId, tr("Cannot open file: %1").arg(filePath));
        delete m_sendFile;
        m_sendFile = nullptr;
        return;
    }
    
    m_sendTransferId = transferId;
    m_sendTotalSize = fileInfo.size();
    m_sendBytesSent = 0;
    
    QCryptographicHash fileHash(QCryptographicHash::Sha256);
    fileHash.addData(m_sendFile);
    m_sendFile->reset();
    
    TransferHeader header;
    header.type = TransferType::FILE_HEADER;
    header.transferId = transferId;
    header.fileName = fileInfo.fileName();
    header.relativePath = relativePath.isEmpty() ? fileInfo.fileName() : relativePath;
    header.fileSize = m_sendTotalSize;
    header.totalFiles = totalFiles;
    header.currentFileIndex = currentFile;
    header.fileHash = fileHash.result();
    
    sendHeader(header);
    m_state = State::Transferring;
    
    QTimer::singleShot(0, this, &TransferSession::sendNextChunk);
}

void TransferSession::sendFolder(const QString& folderPath, const QString& transferId)
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        emit transferFailed(transferId, tr("Folder not found: %1").arg(folderPath));
        return;
    }
    
    QStringList files;
    QDirIterator it(folderPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        files.append(it.next());
    }
    
    if (files.isEmpty()) {
        emit transferFailed(transferId, tr("Folder is empty: %1").arg(folderPath));
        return;
    }
    
    TransferHeader header;
    header.type = TransferType::FOLDER_HEADER;
    header.transferId = transferId;
    header.fileName = dir.dirName();
    header.totalFiles = files.size();
    
    sendHeader(header);
    
    m_pendingSendFiles = files;
    m_pendingFolderTransferId = transferId;
    m_pendingFolderBasePath = folderPath;
    m_pendingFileIndex = 0;
    
    sendNextQueuedFile();
}

void TransferSession::sendNextQueuedFile()
{
    if (m_pendingSendFiles.isEmpty()) return;
    
    QString filePath = m_pendingSendFiles.takeFirst();
    m_pendingFileIndex++;
    
    QDir dir(m_pendingFolderBasePath);
    QString relativePath = dir.dirName() + "/" + 
                          QDir(m_pendingFolderBasePath).relativeFilePath(filePath);
    
    QString fileTransferId = m_pendingFolderTransferId + "/" + QString::number(m_pendingFileIndex);
    
    sendFile(filePath, fileTransferId, relativePath,
             m_pendingSendFiles.size() + 1, m_pendingFileIndex);
}

void TransferSession::cancelTransfer()
{
    TransferHeader header;
    header.type = TransferType::TRANSFER_CANCEL;
    header.transferId = m_currentTransferId.isEmpty() ? m_sendTransferId : m_currentTransferId;
    
    sendHeader(header);
    
    if (m_currentFile) {
        m_currentFile->close();
        m_currentFile->remove();
        delete m_currentFile;
        m_currentFile = nullptr;
    }
    
    if (m_sendFile) {
        m_sendFile->close();
        delete m_sendFile;
        m_sendFile = nullptr;
    }
    
    m_state = State::Idle;
}

void TransferSession::disconnectFromPeer()
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
}

void TransferSession::sendHeader(const TransferHeader& header)
{
    writeMessage(header.toJson(), true);
}

void TransferSession::writeMessage(const QByteArray& data, bool isHeader)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) return;
    
    // Message format: [4 bytes size][1 byte type (0=header, 1=data)][data]
    QByteArray message;
    QDataStream stream(&message, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<qint32>(data.size() + 1);
    stream << static_cast<quint8>(isHeader ? 0 : 1);
    message.append(data);
    
    m_socket->write(message);
}

void TransferSession::sendNextChunk()
{
    if (!m_sendFile || !m_sendFile->isOpen()) return;
    
    QByteArray chunk = m_sendFile->read(CHUNK_SIZE);
    if (chunk.isEmpty()) {
        TransferHeader header;
        header.type = TransferType::FILE_COMPLETE;
        header.transferId = m_sendTransferId;
        sendHeader(header);
        
        m_sendFile->close();
        delete m_sendFile;
        m_sendFile = nullptr;
        
        if (!m_pendingSendFiles.isEmpty()) {
            QTimer::singleShot(0, this, &TransferSession::sendNextQueuedFile);
            return;
        }
        
        emit transferCompleted(m_sendTransferId);
        m_state = State::Completed;
        return;
    }
    
    writeMessage(chunk, false);
    m_sendBytesSent += chunk.size();
    
    emit transferProgress(m_sendTransferId, m_sendBytesSent, m_sendTotalSize);
    
    // Continue sending
    QTimer::singleShot(0, this, &TransferSession::sendNextChunk);
}

void TransferSession::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    
    while (m_buffer.size() >= 4) {
        // Read message size
        if (m_expectedSize == 0) {
            QDataStream stream(m_buffer.left(4));
            stream.setByteOrder(QDataStream::BigEndian);
            stream >> m_expectedSize;
            
            if (m_expectedSize <= 0 || m_expectedSize > MAX_MESSAGE_SIZE) {
                emit error(tr("Invalid message size received from peer"));
                m_buffer.clear();
                m_expectedSize = 0;
                break;
            }
        }
        
        // Check if we have the complete message
        if (m_buffer.size() < 4 + m_expectedSize) break;
        
        // Extract message
        quint8 messageType = static_cast<quint8>(m_buffer.at(4));
        QByteArray messageData = m_buffer.mid(5, m_expectedSize - 1);
        m_buffer.remove(0, 4 + m_expectedSize);
        m_expectedSize = 0;
        
        if (messageType == 0) {
            // Header message
            processMessage(messageData);
        } else {
            // Data message (file chunk)
            handleFileData(messageData);
        }
    }
}

void TransferSession::processMessage(const QByteArray& message)
{
    TransferHeader header = TransferHeader::fromJson(message);
    
    if (header.type == TransferType::CONNECTION_REQUEST) {
        handleConnectionRequest(header);
    } else if (header.type == TransferType::CONNECTION_ACCEPT) {
        handleConnectionAccept(header);
    } else if (header.type == TransferType::CONNECTION_REJECT) {
        handleConnectionReject(header);
    } else if (header.type == TransferType::FILE_HEADER) {
        handleFileHeader(header);
    } else if (header.type == TransferType::FILE_COMPLETE) {
        handleFileComplete(header);
    } else if (header.type == TransferType::TRANSFER_CANCEL) {
        handleTransferCancel(header);
    }
}

void TransferSession::handleConnectionRequest(const TransferHeader& header)
{
    m_peerName = header.senderName.left(MAX_DISPLAY_NAME_LENGTH);
    m_peerId = header.transferId;
    m_isIncoming = true;
    m_verificationCode = header.verificationCode;
    emit connectionRequestReceived(m_peerName, header.transferId);
}

void TransferSession::handleConnectionAccept(const TransferHeader& header)
{
    Q_UNUSED(header)
    m_state = State::Accepted;
    emit connectionAccepted();
}

QString TransferSession::generateVerificationCode() const
{
    int code = QRandomGenerator::global()->bounded(100000, 999999);
    return QString::number(code);
}

void TransferSession::startServerEncryption(const QSslConfiguration& config)
{
    QSslSocket* sslSocket = qobject_cast<QSslSocket*>(m_socket);
    if (sslSocket) {
        sslSocket->setSslConfiguration(config);
        sslSocket->startServerEncryption();
    }
}

void TransferSession::handleConnectionReject(const TransferHeader& header)
{
    Q_UNUSED(header)
    m_state = State::Rejected;
    emit connectionRejected();
}

void TransferSession::handleFileHeader(const TransferHeader& header)
{
    m_currentTransferId = header.transferId;
    m_currentFileName = header.fileName;
    m_currentRelativePath = header.relativePath;
    m_currentFileSize = header.fileSize;
    m_currentBytesReceived = 0;
    m_totalFiles = header.totalFiles;
    m_currentFileIndex = header.currentFileIndex;
    m_expectedHash = header.fileHash;
    m_runningHash.reset();
    
    if (m_maxFileSize > 0 && m_currentFileSize > m_maxFileSize) {
        emit transferFailed(m_currentTransferId,
                           tr("File exceeds maximum size limit (%1 GB)")
                           .arg(m_maxFileSize / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1));
        return;
    }
    
    // Create destination path
    QString destPath = m_downloadPath;
    if (destPath.isEmpty()) {
        destPath = QDir::homePath() + "/Downloads/Witra";
    }
    
    QDir destDir(destPath);
    if (!destDir.exists()) {
        destDir.mkpath(".");
    }
    
    // Handle relative path for folders
    QString filePath;
    if (m_currentRelativePath.contains('/')) {
        QString cleanPath = QDir::cleanPath(m_currentRelativePath);
        if (cleanPath.startsWith("..") || QDir::isAbsolutePath(cleanPath)) {
            emit transferFailed(m_currentTransferId,
                               tr("Security: invalid file path from peer"));
            return;
        }
        QString subDir = cleanPath.left(cleanPath.lastIndexOf('/'));
        destDir.mkpath(subDir);
        filePath = destDir.absoluteFilePath(cleanPath);
    } else {
        filePath = destDir.absoluteFilePath(m_currentFileName);
    }
    
    // Handle file name conflicts
    QFileInfo fileInfo(filePath);
    int counter = 1;
    while (QFile::exists(filePath)) {
        filePath = destDir.absoluteFilePath(
            QString("%1 (%2).%3").arg(fileInfo.baseName())
                                 .arg(counter++)
                                 .arg(fileInfo.suffix())
        );
    }
    
    if (m_currentFile) {
        m_currentFile->close();
        delete m_currentFile;
    }
    
    m_currentFile = new QFile(filePath + ".part", this);
    if (!m_currentFile->open(QIODevice::WriteOnly)) {
        emit transferFailed(m_currentTransferId, 
                           tr("Cannot create file: %1").arg(filePath));
        delete m_currentFile;
        m_currentFile = nullptr;
        return;
    }
    
    m_state = State::Transferring;
    emit transferStarted(m_currentTransferId, m_currentFileName, 
                        m_currentFileSize, m_totalFiles);
}

void TransferSession::handleFileData(const QByteArray& data)
{
    if (!m_currentFile || !m_currentFile->isOpen()) {
        return;
    }
    
    m_currentFile->write(data);
    m_currentBytesReceived += data.size();
    m_runningHash.addData(data);
    
    emit transferProgress(m_currentTransferId, m_currentBytesReceived, m_currentFileSize);
}

void TransferSession::handleFileComplete(const TransferHeader& header)
{
    Q_UNUSED(header)
    
    if (m_currentFile) {
        QString partPath = m_currentFile->fileName();
        m_currentFile->close();
        
        QByteArray receivedHash = m_runningHash.result();
        
        if (m_expectedHash.isEmpty()) {
            emit transferFailed(m_currentTransferId,
                               tr("File integrity not verified - no hash provided by sender"));
            m_currentFile->remove();
            delete m_currentFile;
            m_currentFile = nullptr;
            return;
        }
        
        if (receivedHash != m_expectedHash) {
            m_currentFile->remove();
            delete m_currentFile;
            m_currentFile = nullptr;
            emit transferFailed(m_currentTransferId,
                               tr("File integrity check failed - hash mismatch"));
            return;
        }
        
        delete m_currentFile;
        m_currentFile = nullptr;
        
        QString finalPath = partPath.left(partPath.length() - 5);
        QFile::rename(partPath, finalPath);
        
        emit fileReceived(m_currentTransferId, finalPath);
        
        if (m_currentFileIndex >= m_totalFiles) {
            emit transferCompleted(m_currentTransferId);
            m_state = State::Completed;
        }
    }
}

void TransferSession::handleTransferCancel(const TransferHeader& header)
{
    if (m_currentFile) {
        m_currentFile->close();
        m_currentFile->remove();
        delete m_currentFile;
        m_currentFile = nullptr;
    }
    
    emit transferFailed(header.transferId, tr("Transfer cancelled by peer"));
    m_state = State::Idle;
}

void TransferSession::onDisconnected()
{
    emit disconnected();
}

void TransferSession::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    emit error(m_socket->errorString());
}

} // namespace Witra
