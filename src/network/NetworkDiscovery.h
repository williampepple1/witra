#ifndef NETWORKDISCOVERY_H
#define NETWORKDISCOVERY_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QNetworkInterface>
#include "Protocol.h"

namespace Witra {

class NetworkDiscovery : public QObject {
    Q_OBJECT
    
public:
    explicit NetworkDiscovery(QObject* parent = nullptr);
    ~NetworkDiscovery();
    
    void start(const QString& peerId, const QString& displayName, quint16 transferPort);
    void stop();
    bool isRunning() const { return m_running; }
    
    void setDisplayName(const QString& name) { m_displayName = name; }
    QString displayName() const { return m_displayName; }
    QString peerId() const { return m_peerId; }
    
    void sendGoodbye();
    
signals:
    void peerDiscovered(const QString& peerId, const QString& displayName, 
                       const QString& deviceName, const QHostAddress& address, quint16 port);
    void peerGoodbye(const QString& peerId);
    void error(const QString& errorMessage);
    
private slots:
    void broadcastAnnounce();
    void readPendingDatagrams();
    
private:
    void broadcast(const DiscoveryMessage& message);
    QList<QHostAddress> getBroadcastAddresses() const;
    QString getDeviceName() const;
    
    QUdpSocket* m_socket;
    QTimer* m_broadcastTimer;
    QString m_peerId;
    QString m_displayName;
    QString m_deviceName;
    quint16 m_transferPort;
    bool m_running;
};

} // namespace Witra

#endif // NETWORKDISCOVERY_H
