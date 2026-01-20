#include "Peer.h"

namespace Witra {

Peer::Peer(QObject* parent)
    : QObject(parent)
    , m_port(0)
    , m_state(ConnectionState::Discovered)
    , m_lastSeen(QDateTime::currentDateTime())
{
}

Peer::Peer(const QString& id, const QString& displayName, const QHostAddress& address,
           quint16 port, QObject* parent)
    : QObject(parent)
    , m_id(id)
    , m_displayName(displayName)
    , m_address(address)
    , m_port(port)
    , m_state(ConnectionState::Discovered)
    , m_lastSeen(QDateTime::currentDateTime())
{
}

void Peer::setState(ConnectionState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

void Peer::updateLastSeen()
{
    m_lastSeen = QDateTime::currentDateTime();
}

bool Peer::hasTimedOut(int timeoutMs) const
{
    return m_lastSeen.msecsTo(QDateTime::currentDateTime()) > timeoutMs;
}

QString Peer::stateString() const
{
    switch (m_state) {
        case ConnectionState::Discovered: return tr("Available");
        case ConnectionState::RequestSent: return tr("Request Sent");
        case ConnectionState::RequestReceived: return tr("Request Received");
        case ConnectionState::Connected: return tr("Connected");
        case ConnectionState::Disconnected: return tr("Disconnected");
        default: return tr("Unknown");
    }
}

} // namespace Witra
