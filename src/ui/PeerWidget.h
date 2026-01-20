#ifndef PEERWIDGET_H
#define PEERWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include "core/Peer.h"
#include "core/TransferManager.h"

namespace Witra {

class PeerWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit PeerWidget(Peer* peer, TransferManager* transferManager, 
                        QWidget* parent = nullptr);
    
    void updateDisplay();
    void setPendingFiles(const QStringList& files) { m_pendingFiles = files; }
    
private slots:
    void onConnectClicked();
    void onAcceptClicked();
    void onRejectClicked();
    void onSendFilesClicked();
    void onSendFolderClicked();
    
private:
    void setupUi();
    void applyStyles();
    QString getInitials() const;
    QString getAvatarColor() const;
    
    Peer* m_peer;
    TransferManager* m_transferManager;
    
    QLabel* m_avatarLabel;
    QLabel* m_nameLabel;
    QLabel* m_deviceLabel;
    QLabel* m_statusLabel;
    
    QPushButton* m_connectButton;
    QPushButton* m_acceptButton;
    QPushButton* m_rejectButton;
    QPushButton* m_sendFilesButton;
    QPushButton* m_sendFolderButton;
    
    QWidget* m_actionContainer;
    QStringList m_pendingFiles;
};

} // namespace Witra

#endif // PEERWIDGET_H
