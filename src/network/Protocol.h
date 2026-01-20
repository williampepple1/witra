#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUuid>

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
    
    QByteArray toJson() const {
        QJsonObject obj;
        obj["type"] = type;
        obj["peerId"] = peerId;
        obj["displayName"] = displayName;
        obj["deviceName"] = deviceName;
        obj["transferPort"] = transferPort;
        obj["protocol"] = "witra-v1";
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
            msg.transferPort = static_cast<quint16>(obj["transferPort"].toInt());
        }
        return msg;
    }
    
    bool isValid() const {
        return !peerId.isEmpty() && !type.isEmpty();
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
