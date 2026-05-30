#include "PeerWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QStyle>
#include <QTimer>
#include <QMessageBox>

namespace Witra {

PeerWidget::PeerWidget(Peer* peer, TransferManager* transferManager, QWidget* parent)
    : QWidget(parent)
    , m_peer(peer)
    , m_transferManager(transferManager)
    , m_avatarLabel(nullptr)
    , m_nameLabel(nullptr)
    , m_deviceLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_connectButton(nullptr)
    , m_acceptButton(nullptr)
    , m_rejectButton(nullptr)
    , m_sendFilesButton(nullptr)
    , m_sendFolderButton(nullptr)
    , m_disconnectButton(nullptr)
    , m_actionContainer(nullptr)
{
    setupUi();
    applyStyles();
    updateDisplay();
    
    connect(m_peer, &Peer::stateChanged, this, &PeerWidget::updateDisplay);
    connect(m_transferManager, &TransferManager::transferAdded, this, [this](TransferItem*) { updateDisconnectButton(); });
    connect(m_transferManager, &TransferManager::transferUpdated, this, [this](TransferItem*) { updateDisconnectButton(); });
    connect(m_transferManager, &TransferManager::transferRemoved, this, [this](const QString&) { updateDisconnectButton(); });
}

void PeerWidget::setupUi()
{
    setObjectName("peerWidget");
    setFixedHeight(80);
    
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 12);
    mainLayout->setSpacing(16);
    
    // Avatar
    m_avatarLabel = new QLabel();
    m_avatarLabel->setObjectName("avatar");
    m_avatarLabel->setFixedSize(48, 48);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_avatarLabel);
    
    // Info
    QVBoxLayout* infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);
    
    m_nameLabel = new QLabel();
    m_nameLabel->setObjectName("peerName");
    
    QHBoxLayout* detailsLayout = new QHBoxLayout();
    detailsLayout->setSpacing(8);
    
    m_deviceLabel = new QLabel();
    m_deviceLabel->setObjectName("deviceName");
    
    m_statusLabel = new QLabel();
    m_statusLabel->setObjectName("statusLabel");
    
    detailsLayout->addWidget(m_deviceLabel);
    detailsLayout->addWidget(new QLabel("•"));
    detailsLayout->addWidget(m_statusLabel);
    detailsLayout->addStretch();
    
    infoLayout->addWidget(m_nameLabel);
    infoLayout->addLayout(detailsLayout);
    
    mainLayout->addLayout(infoLayout, 1);
    
    // Actions container
    m_actionContainer = new QWidget();
    QHBoxLayout* actionLayout = new QHBoxLayout(m_actionContainer);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(8);
    
    // Connect button
    m_connectButton = new QPushButton("Connect");
    m_connectButton->setObjectName("connectButton");
    connect(m_connectButton, &QPushButton::clicked, this, &PeerWidget::onConnectClicked);
    actionLayout->addWidget(m_connectButton);
    
    // H10: Removed non-functional Accept/Reject buttons (handled by ConnectionDialog)
    
    // Send buttons (when connected)
    m_sendFilesButton = new QPushButton("Send Files");
    m_sendFilesButton->setObjectName("sendButton");
    connect(m_sendFilesButton, &QPushButton::clicked, this, &PeerWidget::onSendFilesClicked);
    actionLayout->addWidget(m_sendFilesButton);
    
    m_sendFolderButton = new QPushButton("Send Folder");
    m_sendFolderButton->setObjectName("sendButton");
    connect(m_sendFolderButton, &QPushButton::clicked, this, &PeerWidget::onSendFolderClicked);
    actionLayout->addWidget(m_sendFolderButton);
    
    // Disconnect button (when connected and no active transfers)
    m_disconnectButton = new QPushButton("Disconnect");
    m_disconnectButton->setObjectName("disconnectButton");
    connect(m_disconnectButton, &QPushButton::clicked, this, &PeerWidget::onDisconnectClicked);
    actionLayout->addWidget(m_disconnectButton);
    
    mainLayout->addWidget(m_actionContainer);
}

void PeerWidget::applyStyles()
{
    setStyleSheet(R"(
        #peerWidget {
            background-color: #161B22;
            border: 1px solid #30363D;
            border-radius: 12px;
        }
        
        #peerWidget:hover {
            border-color: #484F58;
            background-color: #1C2128;
        }
        
        #avatar {
            background-color: #238636;
            border-radius: 24px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 16px;
            font-weight: 600;
            color: white;
        }
        
        #peerName {
            font-family: 'Segoe UI', sans-serif;
            font-size: 15px;
            font-weight: 600;
            color: #F0F6FC;
        }
        
        #deviceName, QLabel {
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
            color: #8B949E;
        }
        
        #statusLabel {
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
            color: #E8C87A;
        }
        
        #connectButton {
            background-color: #E8C87A;
            border: none;
            border-radius: 8px;
            padding: 8px 20px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            font-weight: 600;
            color: #0D1117;
        }
        
        #connectButton:hover {
            background-color: #F0D898;
        }
        
        #connectButton:disabled {
            background-color: #30363D;
            color: #8B949E;
        }
        
        #acceptButton {
            background-color: #238636;
            border: none;
            border-radius: 8px;
            padding: 8px 20px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            font-weight: 600;
            color: white;
        }
        
        #acceptButton:hover {
            background-color: #2EA043;
        }
        
        #rejectButton {
            background-color: transparent;
            border: 1px solid #F85149;
            border-radius: 8px;
            padding: 8px 20px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            font-weight: 600;
            color: #F85149;
        }
        
        #rejectButton:hover {
            background-color: #F85149;
            color: white;
        }
        
        #sendButton {
            background-color: #238636;
            border: none;
            border-radius: 8px;
            padding: 8px 16px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            font-weight: 500;
            color: white;
        }
        
        #sendButton:hover {
            background-color: #2EA043;
        }
        
        #disconnectButton {
            background-color: transparent;
            border: 1px solid #F85149;
            border-radius: 8px;
            padding: 8px 16px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            font-weight: 500;
            color: #F85149;
        }
        
        #disconnectButton:hover {
            background-color: #F85149;
            color: white;
        }
        
        #disconnectButton:disabled {
            border-color: #484F58;
            color: #484F58;
        }
    )");
}

void PeerWidget::updateDisplay()
{
    // Avatar
    m_avatarLabel->setText(getInitials());
    m_avatarLabel->setStyleSheet(QString(
        "background-color: %1; border-radius: 24px; font-family: 'Segoe UI'; "
        "font-size: 16px; font-weight: 600; color: white;"
    ).arg(getAvatarColor()));
    
    // Name and device
    m_nameLabel->setText(m_peer->displayName());
    m_deviceLabel->setText(m_peer->deviceName());
    
    // Status
    m_statusLabel->setText(m_peer->stateString());
    
    // Update status color based on state
    QString statusColor = "#8B949E";
    switch (m_peer->state()) {
        case Peer::ConnectionState::Connected:
            statusColor = "#238636";
            break;
        case Peer::ConnectionState::RequestSent:
        case Peer::ConnectionState::RequestReceived:
            statusColor = "#D29922";
            break;
        case Peer::ConnectionState::Discovered:
            statusColor = "#E8C87A";
            break;
        default:
            break;
    }
    m_statusLabel->setStyleSheet(QString("color: %1;").arg(statusColor));
    
    // Show/hide buttons based on state
    Peer::ConnectionState state = m_peer->state();
    
    m_connectButton->setVisible(state == Peer::ConnectionState::Discovered);
    // H10: Removed Accept/Reject buttons visibility
    m_sendFilesButton->setVisible(state == Peer::ConnectionState::Connected);
    m_sendFolderButton->setVisible(state == Peer::ConnectionState::Connected);
    m_disconnectButton->setVisible(state == Peer::ConnectionState::Connected);
    
    // Update disconnect button state
    updateDisconnectButton();
    
    if (state == Peer::ConnectionState::RequestSent) {
        m_connectButton->setVisible(true);
        m_connectButton->setEnabled(false);
        m_connectButton->setText("Waiting...");
    } else {
        m_connectButton->setEnabled(true);
        m_connectButton->setText("Connect");
    }
}

QString PeerWidget::getInitials() const
{
    QString name = m_peer->displayName();
    if (name.isEmpty()) return "?";
    
    QStringList parts = name.split(' ', Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        return QString("%1%2").arg(parts[0][0].toUpper())
                              .arg(parts[1][0].toUpper());
    }
    return name.left(2).toUpper();
}

QString PeerWidget::getAvatarColor() const
{
    // Generate consistent color from peer ID
    static const QStringList colors = {
        "#238636", "#C49A3C", "#A371F7", "#DB61A2", 
        "#F85149", "#D29922", "#3FB950", "#E8C87A"
    };
    
    uint hash = 0;
    for (const QChar& c : m_peer->id()) {
        hash = c.unicode() + (hash << 6) + (hash << 16) - hash;
    }
    
    return colors[hash % colors.size()];
}

void PeerWidget::onConnectClicked()
{
    m_transferManager->sendConnectionRequest(m_peer);
}

void PeerWidget::onAcceptClicked()
{
}

void PeerWidget::onRejectClicked()
{
}

void PeerWidget::onSendFilesClicked()
{
    if (!m_pendingFiles.isEmpty()) {
        m_transferManager->sendFiles(m_peer, m_pendingFiles);
        m_pendingFiles.clear();
    } else {
        QStringList files = QFileDialog::getOpenFileNames(
            this, tr("Select Files to Send"), 
            QDir::homePath()
        );
        
        if (!files.isEmpty()) {
            m_transferManager->sendFiles(m_peer, files);
        }
    }
}

void PeerWidget::onSendFolderClicked()
{
    QString folder = QFileDialog::getExistingDirectory(
        this, tr("Select Folder to Send"),
        QDir::homePath()
    );
    
    if (!folder.isEmpty()) {
        m_transferManager->sendFolder(m_peer, folder);
    }
}

void PeerWidget::onDisconnectClicked()
{
    if (m_transferManager->hasActiveTransfersWithPeer(m_peer->id())) {
        QMessageBox::warning(this, tr("Cannot Disconnect"),
            tr("Cannot disconnect while file transfers are in progress.\n"
               "Please wait for transfers to complete or cancel them first."));
        return;
    }
    
    m_transferManager->disconnectFromPeer(m_peer);
}

void PeerWidget::updateDisconnectButton()
{
    if (!m_disconnectButton->isVisible()) return;
    
    bool hasActiveTransfers = m_transferManager->hasActiveTransfersWithPeer(m_peer->id());
    m_disconnectButton->setEnabled(!hasActiveTransfers);
    
    if (hasActiveTransfers) {
        m_disconnectButton->setToolTip(tr("Cannot disconnect while transfers are in progress"));
    } else {
        m_disconnectButton->setToolTip(tr("Disconnect from this device"));
    }
}

} // namespace Witra
