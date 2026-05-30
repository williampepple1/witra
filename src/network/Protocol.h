#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUuid>
#include <QMessageAuthenticationCode>
#include <QDateTime>

namespace Witra {

// Network ports
constexpr quint16 DISCOVERY_PORT = 45678;
constexpr quint16 TRANSFER_PORT = 45679;

// Discovery broadcast interval (ms)
constexpr int DISCOVERY_INTERVAL = 3000;

// Peer timeout (ms) - if no broadcast received
constexpr int PEER_TIMEOUT = 10000;

// Buffer sizes
constexpr qint64 CHUNK_SIZE = 65536; // 64KB chunks for file transfer
constexpr qint32 MAX_MESSAGE_SIZE = 1048576; // 1MB max incoming message size (prevents memory exhaustion)

// Security limits
constexpr int MAX_DISPLAY_NAME_LENGTH = 64; // max chars for network-provided display names
constexpr int MAX_CONNECTIONS = 10; // max concurrent incoming connections
constexpr int MAX_PORT_RANGE = 100; // ports to try if default is occupied
constexpr qint64 MAX_FILE_SIZE = 10LL * 1024 * 1024 * 1024; // 10GB max file size
constexpr int VERIFICATION_CODE_LENGTH = 6; // digits in pairing code
constexpr int DISCOVERY_TIMESTAMP_WINDOW = 15; // seconds of tolerance for discovery message timestamps

// Message types for discovery
namespace DiscoveryType {
    constexpr const char* ANNOUNCE = "announce";
    constexpr const char* GOODBYE = "goodbye";
}

// Message types for transfer protocol
namespace TransferType {
    constexpr const char* CONNECTION_REQUEST = "connection_request";
    constexpr const char* CONNECTION_ACCEPT = "connection_accept";
    constexpr const char* CONNECTION_REJECT = "connection_reject";
    constexpr const char* FILE_HEADER = "file_header";
    constexpr const char* FILE_DATA = "file_data";
    constexpr const char* FILE_COMPLETE = "file_complete";
    constexpr const char* FOLDER_HEADER = "folder_header";
    constexpr const char* TRANSFER_CANCEL = "transfer_cancel";
    constexpr const char* TRANSFER_ACK = "transfer_ack";
    constexpr const char* PING = "ping";
    constexpr const char* PONG = "pong";
}

// Discovery message structure
struct DiscoveryMessage {
    QString type;
    QString peerId;
    QString displayName;
    QString deviceName;
    quint16 transferPort;
    qint64 timestamp;
    QByteArray token;
    
    QByteArray toJson() const {
        QJsonObject obj;
        obj["type"] = type;
        obj["peerId"] = peerId;
        obj["displayName"] = displayName;
        obj["deviceName"] = deviceName;
        obj["transferPort"] = transferPort;
        obj["timestamp"] = timestamp;
        obj["token"] = QString::fromLatin1(token.toHex());
        obj["protocol"] = "witra-v2";
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }
    
    static DiscoveryMessage fromJson(const QByteArray& data) {
        DiscoveryMessage msg;
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            msg.type = obj["type"].toString();
            msg.peerId = obj["peerId"].toString();
            msg.displayName = obj["displayName"].toString();
            msg.deviceName = obj["deviceName"].toString();
            int port = obj["transferPort"].toInt();
            if (port > 0 && port <= 65535) {
                msg.transferPort = static_cast<quint16>(port);
            } else {
                msg.transferPort = 0;
            }
            msg.timestamp = obj["timestamp"].toVariant().toLongLong();
            msg.token = QByteArray::fromHex(obj["token"].toString().toLatin1());
        }
        return msg;
    }
    
    bool isValid() const {
        return !peerId.isEmpty() && !type.isEmpty() && transferPort > 0;
    }
};

// Transfer protocol header
struct TransferHeader {
    QString type;
    QString transferId;
    QString fileName;
    QString relativePath;
    qint64 fileSize;
    qint64 totalFiles;
    qint64 currentFileIndex;
    QString senderName;
    QString verificationCode;
    QByteArray fileHash;
    
    QByteArray toJson() const {
        QJsonObject obj;
        obj["type"] = type;
        obj["transferId"] = transferId;
        obj["fileName"] = fileName;
        obj["relativePath"] = relativePath;
        obj["fileSize"] = fileSize;
        obj["totalFiles"] = totalFiles;
        obj["currentFileIndex"] = currentFileIndex;
        obj["senderName"] = senderName;
        obj["verificationCode"] = verificationCode;
        obj["fileHash"] = QString::fromLatin1(fileHash.toHex());
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }
    
    static TransferHeader fromJson(const QByteArray& data) {
        TransferHeader header;
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            header.type = obj["type"].toString();
            header.transferId = obj["transferId"].toString();
            header.fileName = obj["fileName"].toString();
            header.relativePath = obj["relativePath"].toString();
            header.fileSize = obj["fileSize"].toVariant().toLongLong();
            header.totalFiles = obj["totalFiles"].toVariant().toLongLong();
            header.currentFileIndex = obj["currentFileIndex"].toVariant().toLongLong();
            header.senderName = obj["senderName"].toString();
            header.verificationCode = obj["verificationCode"].toString();
            header.fileHash = QByteArray::fromHex(obj["fileHash"].toString().toLatin1());
        }
        return header;
    }
};

// Generate unique ID
inline QString generateUniqueId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace Witra

#endif // PROTOCOL_H
