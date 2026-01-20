#ifndef PEER_H
#define PEER_H

#include <QObject>
#include <QString>
#include <QHostAddress>
#include <QDateTime>

namespace Witra {

class Peer : public QObject {
    Q_OBJECT
    
public:
    enum class ConnectionState {
        Discovered,      // Just found on network
        RequestSent,     // We sent a connection request
        RequestReceived, // They sent us a connection request
        Connected,       // Both accepted, ready to transfer
        Disconnected     // Was connected but lost connection
    };
    Q_ENUM(ConnectionState)
    
    explicit Peer(QObject* parent = nullptr);
    Peer(const QString& id, const QString& displayName, const QHostAddress& address, 
         quint16 port, QObject* parent = nullptr);
    
    // Getters
    QString id() const { return m_id; }
    QString displayName() const { return m_displayName; }
    QString deviceName() const { return m_deviceName; }
    QHostAddress address() const { return m_address; }
    quint16 port() const { return m_port; }
    ConnectionState state() const { return m_state; }
    QDateTime lastSeen() const { return m_lastSeen; }
    
    // Setters
    void setDisplayName(const QString& name) { m_displayName = name; }
    void setDeviceName(const QString& name) { m_deviceName = name; }
    void setAddress(const QHostAddress& address) { m_address = address; }
    void setPort(quint16 port) { m_port = port; }
    void setState(ConnectionState state);
    void updateLastSeen();
    
    // State helpers
    bool isConnected() const { return m_state == ConnectionState::Connected; }
    bool canSendFiles() const { return m_state == ConnectionState::Connected; }
    bool hasTimedOut(int timeoutMs) const;
    
    QString stateString() const;
    
signals:
    void stateChanged(ConnectionState newState);
    
private:
    QString m_id;
    QString m_displayName;
    QString m_deviceName;
    QHostAddress m_address;
    quint16 m_port;
    ConnectionState m_state;
    QDateTime m_lastSeen;
};

} // namespace Witra

#endif // PEER_H
