#include "NetworkDiscovery.h"
#include <QNetworkDatagram>
#include <QHostInfo>
#include <QDateTime>
#include <algorithm>

namespace Witra {

NetworkDiscovery::NetworkDiscovery(QObject* parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
    , m_broadcastTimer(new QTimer(this))
    , m_transferPort(TRANSFER_PORT)
    , m_running(false)
{
    m_deviceName = getDeviceName();
    
    connect(m_socket, &QUdpSocket::readyRead, 
            this, &NetworkDiscovery::readPendingDatagrams);
    connect(m_broadcastTimer, &QTimer::timeout, 
            this, &NetworkDiscovery::broadcastAnnounce);
}

NetworkDiscovery::~NetworkDiscovery()
{
    if (m_running) {
        sendGoodbye();
        stop();
    }
}

void NetworkDiscovery::start(const QString& peerId, const QString& displayName, quint16 transferPort)
{
    if (m_running) return;
    
    m_peerId = peerId;
    m_displayName = displayName;
    m_transferPort = transferPort;
    
    // Bind to discovery port with fallback
    bool bound = false;
    for (int offset = 0; offset < MAX_PORT_RANGE; ++offset) {
        quint16 port = DISCOVERY_PORT + offset;
        if (m_socket->bind(QHostAddress::AnyIPv4, port,
                           QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            bound = true;
            break;
        }
    }
    if (!bound) {
        emit error(tr("Failed to bind to discovery port (tried %1-%2): %3")
                   .arg(DISCOVERY_PORT).arg(DISCOVERY_PORT + MAX_PORT_RANGE - 1)
                   .arg(m_socket->errorString()));
        return;
    }
    
    m_running = true;
    
    // Generate initial discovery token
    m_currentToken = generateToken();
    
    // Start broadcasting
    broadcastAnnounce();
    m_broadcastTimer->start(DISCOVERY_INTERVAL);
}

void NetworkDiscovery::stop()
{
    if (!m_running) return;
    
    m_broadcastTimer->stop();
    m_socket->close();
    m_peerTokens.clear();
    m_running = false;
}

void NetworkDiscovery::sendGoodbye()
{
    if (!m_running) return;
    
    DiscoveryMessage msg;
    msg.type = DiscoveryType::GOODBYE;
    msg.peerId = m_peerId;
    msg.displayName = m_displayName;
    msg.deviceName = m_deviceName;
    msg.transferPort = m_transferPort;
    msg.timestamp = QDateTime::currentSecsSinceEpoch();
    msg.token = m_currentToken;
    
    broadcast(msg);
}

void NetworkDiscovery::broadcastAnnounce()
{
    DiscoveryMessage msg;
    msg.type = DiscoveryType::ANNOUNCE;
    msg.peerId = m_peerId;
    msg.displayName = m_displayName;
    msg.deviceName = m_deviceName;
    msg.transferPort = m_transferPort;
    msg.timestamp = QDateTime::currentSecsSinceEpoch();
    msg.token = m_currentToken;
    
    broadcast(msg);
}

void NetworkDiscovery::broadcast(const DiscoveryMessage& message)
{
    QByteArray data = message.toJson();
    
    for (const QHostAddress& broadcastAddr : getBroadcastAddresses()) {
        m_socket->writeDatagram(data, broadcastAddr, DISCOVERY_PORT);
    }
}

void NetworkDiscovery::readPendingDatagrams()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        
        if (!datagram.isValid()) continue;
        
        DiscoveryMessage msg = DiscoveryMessage::fromJson(datagram.data());
        
        if (!msg.isValid()) continue;
        
        // Ignore our own broadcasts
        if (msg.peerId == m_peerId) continue;
        
        // Reject stale messages
        qint64 now = QDateTime::currentSecsSinceEpoch();
        if (qAbs(now - msg.timestamp) > DISCOVERY_TIMESTAMP_WINDOW) continue;
        
        if (msg.type == DiscoveryType::ANNOUNCE) {
            if (m_peerTokens.size() > 1000) {
                auto it = m_peerTokens.begin();
                for (int i = 0; i < 500 && it != m_peerTokens.end(); ++i) {
                    it = m_peerTokens.erase(it);
                }
            }
            m_peerTokens[msg.peerId] = qMakePair(msg.token, datagram.senderAddress());
            
            QString name = msg.displayName.left(MAX_DISPLAY_NAME_LENGTH);
            QString device = msg.deviceName.left(MAX_DISPLAY_NAME_LENGTH);
            emit peerDiscovered(msg.peerId, name, device,
                               datagram.senderAddress(), msg.transferPort);
        } else if (msg.type == DiscoveryType::GOODBYE) {
            auto it = m_peerTokens.find(msg.peerId);
            if (it == m_peerTokens.end()) continue;
            
            if (it->first != msg.token) continue;
            if (it->second != datagram.senderAddress()) continue;
            
            m_peerTokens.erase(it);
            emit peerGoodbye(msg.peerId);
        }
    }
}

QByteArray NetworkDiscovery::generateToken() const
{
    QByteArray token(16, '\0');
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(token.data()), token.size() / sizeof(quint32));
    return token;
}

QList<QHostAddress> NetworkDiscovery::getBroadcastAddresses() const
{
    QList<QHostAddress> broadcastAddresses;
    
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        // Skip loopback and non-running interfaces
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp)) continue;
        
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                QHostAddress broadcast = entry.broadcast();
                if (!broadcast.isNull() && !broadcastAddresses.contains(broadcast)) {
                    broadcastAddresses.append(broadcast);
                }
            }
        }
    }
    
    // Fallback to generic broadcast if no specific addresses found
    if (broadcastAddresses.isEmpty()) {
        broadcastAddresses.append(QHostAddress::Broadcast);
    }
    
    return broadcastAddresses;
}

QString NetworkDiscovery::getDeviceName() const
{
    return QHostInfo::localHostName();
}

} // namespace Witra
