#include "NetworkDiscovery.h"
#include <QNetworkDatagram>
#include <QHostInfo>

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
    
    // Bind to discovery port
    if (!m_socket->bind(QHostAddress::AnyIPv4, DISCOVERY_PORT, 
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit error(tr("Failed to bind to discovery port %1: %2")
                   .arg(DISCOVERY_PORT).arg(m_socket->errorString()));
        return;
    }
    
    m_running = true;
    
    // Start broadcasting
    broadcastAnnounce();
    m_broadcastTimer->start(DISCOVERY_INTERVAL);
}

void NetworkDiscovery::stop()
{
    if (!m_running) return;
    
    m_broadcastTimer->stop();
    m_socket->close();
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
        
        if (msg.type == DiscoveryType::ANNOUNCE) {
            emit peerDiscovered(msg.peerId, msg.displayName, msg.deviceName,
                               datagram.senderAddress(), msg.transferPort);
        } else if (msg.type == DiscoveryType::GOODBYE) {
            emit peerGoodbye(msg.peerId);
        }
    }
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
