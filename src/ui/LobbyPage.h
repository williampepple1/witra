#ifndef LOBBYPAGE_H
#define LOBBYPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include "core/PeerManager.h"
#include "core/TransferManager.h"

namespace Witra {

class PeerWidget;

class LobbyPage : public QWidget {
    Q_OBJECT
    
public:
    explicit LobbyPage(PeerManager* peerManager, TransferManager* transferManager,
                       QWidget* parent = nullptr);
    
    void setFilesToSend(const QStringList& paths) { m_pendingFiles = paths; }
    
private slots:
    void onPeerAdded(Peer* peer);
    void onPeerRemoved(const QString& peerId);
    void onPeerUpdated(Peer* peer);
    void onDisplayNameChanged();
    void updatePeerList();
    
private:
    void setupUi();
    void applyStyles();
    
    PeerManager* m_peerManager;
    TransferManager* m_transferManager;
    
    QLineEdit* m_displayNameEdit;
    QWidget* m_peersContainer;
    QVBoxLayout* m_peersLayout;
    QLabel* m_emptyLabel;
    
    QMap<QString, PeerWidget*> m_peerWidgets;
    QStringList m_pendingFiles;
};

} // namespace Witra

#endif // LOBBYPAGE_H
