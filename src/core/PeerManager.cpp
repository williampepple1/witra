#include "PeerManager.h"
#include <QSettings>

namespace Witra {

PeerManager::PeerManager(QObject* parent)
    : QObject(parent)
    , m_discovery(new NetworkDiscovery(this))
    , m_cleanupTimer(new QTimer(this))
    , m_peerId(generateUniqueId())
    , m_running(false)
{
    // Load or generate display name
    QSettings settings;
    m_displayName = settings.value("user/displayName", 
                                   QSysInfo::machineHostName()).toString();
    
    // Connect discovery signals
    connect(m_discovery, &NetworkDiscovery::peerDiscovered,
            this, &PeerManager::onPeerDiscovered);
    connect(m_discovery, &NetworkDiscovery::peerGoodbye,
            this, &PeerManager::onPeerGoodbye);
    connect(m_discovery, &NetworkDiscovery::error,
            this, &PeerManager::error);
    
    // Cleanup timer for timed out peers
    connect(m_cleanupTimer, &QTimer::timeout,
            this, &PeerManager::cleanupTimedOutPeers);
}

PeerManager::~PeerManager()
{
    stop();
}

void PeerManager::start()
{
    if (m_running) return;
    
    m_discovery->start(m_peerId, m_displayName, TRANSFER_PORT);
    m_cleanupTimer->start(PEER_TIMEOUT / 2);
    m_running = true;
}

void PeerManager::stop()
{
    if (!m_running) return;
    
    m_cleanupTimer->stop();
    m_discovery->stop();
    
    // Clear all peers
    for (const QString& id : m_peers.keys()) {
        removePeer(id);
    }
    
    m_running = false;
}

void PeerManager::setDisplayName(const QString& name)
{
    m_displayName = name;
    
    // Save to settings
    QSettings settings;
    settings.setValue("user/displayName", name);
    
    // Update discovery
    m_discovery->setDisplayName(name);
}

void PeerManager::onPeerDiscovered(const QString& peerId, const QString& displayName,
                                   const QString& deviceName, const QHostAddress& address, 
                                   quint16 port)
{
    if (m_peers.contains(peerId)) {
        // Update existing peer
        Peer* peer = m_peers[peerId];
        peer->setDisplayName(displayName);
        peer->setDeviceName(deviceName);
        peer->setAddress(address);
        peer->setPort(port);
        peer->updateLastSeen();
        emit peerUpdated(peer);
    } else {
        // Add new peer
        Peer* peer = new Peer(peerId, displayName, address, port, this);
        peer->setDeviceName(deviceName);
        m_peers[peerId] = peer;
        emit peerAdded(peer);
    }
}

void PeerManager::onPeerGoodbye(const QString& peerId)
{
    removePeer(peerId);
}

void PeerManager::cleanupTimedOutPeers()
{
    QStringList timedOut;
    
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (it.value()->hasTimedOut(PEER_TIMEOUT)) {
            timedOut.append(it.key());
        }
    }
    
    for (const QString& id : timedOut) {
        removePeer(id);
    }
}

void PeerManager::removePeer(const QString& peerId)
{
    if (m_peers.contains(peerId)) {
        Peer* peer = m_peers.take(peerId);
        emit peerRemoved(peerId);
        peer->deleteLater();
    }
}

void PeerManager::sendConnectionRequest(const QString& peerId)
{
    Peer* peer = m_peers.value(peerId, nullptr);
    if (peer && peer->state() == Peer::ConnectionState::Discovered) {
        peer->setState(Peer::ConnectionState::RequestSent);
        emit peerUpdated(peer);
        // The actual request is sent via TransferManager
    }
}

void PeerManager::acceptConnectionRequest(const QString& peerId)
{
    Peer* peer = m_peers.value(peerId, nullptr);
    if (peer && peer->state() == Peer::ConnectionState::RequestReceived) {
        peer->setState(Peer::ConnectionState::Connected);
        emit connectionAccepted(peer);
        emit peerUpdated(peer);
    }
}

void PeerManager::rejectConnectionRequest(const QString& peerId)
{
    Peer* peer = m_peers.value(peerId, nullptr);
    if (peer && peer->state() == Peer::ConnectionState::RequestReceived) {
        peer->setState(Peer::ConnectionState::Discovered);
        emit connectionRejected(peer);
        emit peerUpdated(peer);
    }
}

void PeerManager::disconnectPeer(const QString& peerId)
{
    Peer* peer = m_peers.value(peerId, nullptr);
    if (peer && peer->state() == Peer::ConnectionState::Connected) {
        peer->setState(Peer::ConnectionState::Discovered);
        emit peerUpdated(peer);
    }
}

} // namespace Witra
