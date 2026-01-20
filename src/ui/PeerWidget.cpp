#include "PeerWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QStyle>

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
    , m_actionContainer(nullptr)
{
    setupUi();
    applyStyles();
    updateDisplay();
    
    connect(m_peer, &Peer::stateChanged, this, &PeerWidget::updateDisplay);
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
    
    // Accept/Reject buttons (for incoming requests)
    m_acceptButton = new QPushButton("Accept");
    m_acceptButton->setObjectName("acceptButton");
    connect(m_acceptButton, &QPushButton::clicked, this, &PeerWidget::onAcceptClicked);
    actionLayout->addWidget(m_acceptButton);
    
    m_rejectButton = new QPushButton("Reject");
    m_rejectButton->setObjectName("rejectButton");
    connect(m_rejectButton, &QPushButton::clicked, this, &PeerWidget::onRejectClicked);
    actionLayout->addWidget(m_rejectButton);
    
    // Send buttons (when connected)
    m_sendFilesButton = new QPushButton("Send Files");
    m_sendFilesButton->setObjectName("sendButton");
    connect(m_sendFilesButton, &QPushButton::clicked, this, &PeerWidget::onSendFilesClicked);
    actionLayout->addWidget(m_sendFilesButton);
    
    m_sendFolderButton = new QPushButton("Send Folder");
    m_sendFolderButton->setObjectName("sendButton");
    connect(m_sendFolderButton, &QPushButton::clicked, this, &PeerWidget::onSendFolderClicked);
    actionLayout->addWidget(m_sendFolderButton);
    
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
            color: #00D9FF;
        }
        
        #connectButton {
            background-color: #00D9FF;
            border: none;
            border-radius: 8px;
            padding: 8px 20px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            font-weight: 600;
            color: #0D1117;
        }
        
        #connectButton:hover {
            background-color: #33E1FF;
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
            statusColor = "#00D9FF";
            break;
        default:
            break;
    }
    m_statusLabel->setStyleSheet(QString("color: %1;").arg(statusColor));
    
    // Show/hide buttons based on state
    Peer::ConnectionState state = m_peer->state();
    
    m_connectButton->setVisible(state == Peer::ConnectionState::Discovered);
    m_acceptButton->setVisible(state == Peer::ConnectionState::RequestReceived);
    m_rejectButton->setVisible(state == Peer::ConnectionState::RequestReceived);
    m_sendFilesButton->setVisible(state == Peer::ConnectionState::Connected);
    m_sendFolderButton->setVisible(state == Peer::ConnectionState::Connected);
    
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
        "#238636", "#1F6FEB", "#A371F7", "#DB61A2", 
        "#F85149", "#D29922", "#3FB950", "#58A6FF"
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
    // Find the session for this peer
    // This is handled by MainWindow's connection dialog
}

void PeerWidget::onRejectClicked()
{
    // Handled by MainWindow's connection dialog
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

} // namespace Witra
