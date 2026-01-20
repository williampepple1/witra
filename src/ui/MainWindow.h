#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include "core/PeerManager.h"
#include "core/TransferManager.h"

namespace Witra {

class LobbyPage;
class TransferPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    
private slots:
    void showLobbyPage();
    void showTransferPage();
    void onConnectionRequestReceived(TransferSession* session, const QString& senderName);
    void onError(const QString& error);
    
private:
    void setupUi();
    void setupTrayIcon();
    void applyStyles();
    
    PeerManager* m_peerManager;
    TransferManager* m_transferManager;
    
    QStackedWidget* m_stackedWidget;
    LobbyPage* m_lobbyPage;
    TransferPage* m_transferPage;
    
    QSystemTrayIcon* m_trayIcon;
};

} // namespace Witra

#endif // MAINWINDOW_H
