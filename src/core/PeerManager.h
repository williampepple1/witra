#ifndef PEERMANAGER_H
#define PEERMANAGER_H

#include <QObject>
#include <QMap>
#include <QTimer>
#include "Peer.h"
#include "network/NetworkDiscovery.h"
#include "network/Protocol.h"

namespace Witra {

class PeerManager : public QObject {
    Q_OBJECT
    
public:
    explicit PeerManager(QObject* parent = nullptr);
    ~PeerManager();
    
    void start();
    void stop();
    bool isRunning() const { return m_running; }
    
    // Settings
    void setDisplayName(const QString& name);
    QString displayName() const { return m_displayName; }
    QString peerId() const { return m_peerId; }
    
    // Peer access
    QList<Peer*> peers() const { return m_peers.values(); }
    Peer* peer(const QString& id) const { return m_peers.value(id, nullptr); }
    int peerCount() const { return m_peers.size(); }
    
    // Connection management
    void sendConnectionRequest(const QString& peerId);
    void acceptConnectionRequest(const QString& peerId);
    void rejectConnectionRequest(const QString& peerId);
    void disconnectPeer(const QString& peerId);
    
signals:
    void peerAdded(Peer* peer);
    void peerRemoved(const QString& peerId);
    void peerUpdated(Peer* peer);
    void connectionRequestReceived(Peer* peer);
    void connectionAccepted(Peer* peer);
    void connectionRejected(Peer* peer);
    void error(const QString& errorMessage);
    
private slots:
    void onPeerDiscovered(const QString& peerId, const QString& displayName,
                         const QString& deviceName, const QHostAddress& address, quint16 port);
    void onPeerGoodbye(const QString& peerId);
    void cleanupTimedOutPeers();
    
private:
    void removePeer(const QString& peerId);
    
    NetworkDiscovery* m_discovery;
    QMap<QString, Peer*> m_peers;
    QTimer* m_cleanupTimer;
    QString m_peerId;
    QString m_displayName;
    bool m_running;
};

} // namespace Witra

#endif // PEERMANAGER_H
