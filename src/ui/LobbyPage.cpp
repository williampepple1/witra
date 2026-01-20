#include "LobbyPage.h"
#include "PeerWidget.h"

#include <QScrollArea>
#include <QFileDialog>
#include <QPushButton>
#include <QGridLayout>

namespace Witra {

LobbyPage::LobbyPage(PeerManager* peerManager, TransferManager* transferManager,
                     QWidget* parent)
    : QWidget(parent)
    , m_peerManager(peerManager)
    , m_transferManager(transferManager)
    , m_displayNameEdit(nullptr)
    , m_peersContainer(nullptr)
    , m_peersLayout(nullptr)
    , m_emptyLabel(nullptr)
{
    setupUi();
    applyStyles();
    
    // Connect signals
    connect(m_peerManager, &PeerManager::peerAdded, this, &LobbyPage::onPeerAdded);
    connect(m_peerManager, &PeerManager::peerRemoved, this, &LobbyPage::onPeerRemoved);
    connect(m_peerManager, &PeerManager::peerUpdated, this, &LobbyPage::onPeerUpdated);
    
    // Load existing peers
    updatePeerList();
}

void LobbyPage::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(32, 24, 32, 24);
    mainLayout->setSpacing(24);
    
    // Settings section
    QWidget* settingsCard = new QWidget();
    settingsCard->setObjectName("settingsCard");
    
    QVBoxLayout* settingsLayout = new QVBoxLayout(settingsCard);
    settingsLayout->setContentsMargins(20, 16, 20, 16);
    settingsLayout->setSpacing(12);
    
    QLabel* settingsTitle = new QLabel("Your Profile");
    settingsTitle->setObjectName("cardTitle");
    
    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->setSpacing(12);
    
    QLabel* nameLabel = new QLabel("Display Name:");
    nameLabel->setObjectName("fieldLabel");
    
    m_displayNameEdit = new QLineEdit();
    m_displayNameEdit->setObjectName("nameEdit");
    m_displayNameEdit->setText(m_peerManager->displayName());
    m_displayNameEdit->setPlaceholderText("Enter your name...");
    m_displayNameEdit->setMaxLength(32);
    
    connect(m_displayNameEdit, &QLineEdit::editingFinished, 
            this, &LobbyPage::onDisplayNameChanged);
    
    nameLayout->addWidget(nameLabel);
    nameLayout->addWidget(m_displayNameEdit, 1);
    
    settingsLayout->addWidget(settingsTitle);
    settingsLayout->addLayout(nameLayout);
    
    mainLayout->addWidget(settingsCard);
    
    // Devices section header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QLabel* devicesTitle = new QLabel("Devices on Network");
    devicesTitle->setObjectName("sectionTitle");
    
    QLabel* countLabel = new QLabel();
    countLabel->setObjectName("countLabel");
    
    auto updateCount = [this, countLabel]() {
        int count = m_peerManager->peerCount();
        countLabel->setText(QString("(%1)").arg(count));
    };
    
    connect(m_peerManager, &PeerManager::peerAdded, countLabel, updateCount);
    connect(m_peerManager, &PeerManager::peerRemoved, countLabel, updateCount);
    updateCount();
    
    headerLayout->addWidget(devicesTitle);
    headerLayout->addWidget(countLabel);
    headerLayout->addStretch();
    
    // Refresh indicator
    QLabel* refreshLabel = new QLabel("Auto-discovering devices...");
    refreshLabel->setObjectName("refreshLabel");
    headerLayout->addWidget(refreshLabel);
    
    mainLayout->addLayout(headerLayout);
    
    // Peers grid in scroll area
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setObjectName("peersScroll");
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    m_peersContainer = new QWidget();
    m_peersContainer->setObjectName("peersContainer");
    
    m_peersLayout = new QVBoxLayout(m_peersContainer);
    m_peersLayout->setContentsMargins(0, 0, 0, 0);
    m_peersLayout->setSpacing(12);
    m_peersLayout->addStretch();
    
    // Empty state
    m_emptyLabel = new QLabel();
    m_emptyLabel->setObjectName("emptyLabel");
    m_emptyLabel->setText(
        "<div style='text-align: center;'>"
        "<p style='font-size: 48px; margin-bottom: 16px;'>📡</p>"
        "<p style='font-size: 16px; color: #8B949E; margin-bottom: 8px;'>No devices found</p>"
        "<p style='font-size: 13px; color: #484F58;'>Make sure other devices running Witra<br/>are connected to the same WiFi network</p>"
        "</div>"
    );
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_peersLayout->insertWidget(0, m_emptyLabel);
    
    scrollArea->setWidget(m_peersContainer);
    mainLayout->addWidget(scrollArea, 1);
}

void LobbyPage::applyStyles()
{
    setStyleSheet(R"(
        LobbyPage {
            background-color: #0D1117;
        }
        
        #settingsCard {
            background-color: #161B22;
            border: 1px solid #30363D;
            border-radius: 12px;
        }
        
        #cardTitle {
            font-family: 'Segoe UI', sans-serif;
            font-size: 14px;
            font-weight: 600;
            color: #F0F6FC;
        }
        
        #fieldLabel {
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            color: #8B949E;
        }
        
        #nameEdit {
            background-color: #0D1117;
            border: 1px solid #30363D;
            border-radius: 8px;
            padding: 10px 14px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 14px;
            color: #F0F6FC;
        }
        
        #nameEdit:focus {
            border-color: #00D9FF;
            outline: none;
        }
        
        #sectionTitle {
            font-family: 'Segoe UI', sans-serif;
            font-size: 18px;
            font-weight: 600;
            color: #F0F6FC;
        }
        
        #countLabel {
            font-family: 'Segoe UI', sans-serif;
            font-size: 18px;
            font-weight: 400;
            color: #484F58;
        }
        
        #refreshLabel {
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
            color: #00D9FF;
        }
        
        #peersScroll {
            background-color: transparent;
        }
        
        #peersContainer {
            background-color: transparent;
        }
        
        #emptyLabel {
            padding: 48px;
        }
    )");
}

void LobbyPage::onPeerAdded(Peer* peer)
{
    if (m_peerWidgets.contains(peer->id())) return;
    
    PeerWidget* widget = new PeerWidget(peer, m_transferManager, this);
    m_peerWidgets[peer->id()] = widget;
    
    // Insert before stretch
    m_peersLayout->insertWidget(m_peersLayout->count() - 1, widget);
    
    // Set pending files if any
    if (!m_pendingFiles.isEmpty()) {
        widget->setPendingFiles(m_pendingFiles);
    }
    
    // Hide empty label
    m_emptyLabel->setVisible(false);
}

void LobbyPage::onPeerRemoved(const QString& peerId)
{
    if (m_peerWidgets.contains(peerId)) {
        PeerWidget* widget = m_peerWidgets.take(peerId);
        m_peersLayout->removeWidget(widget);
        widget->deleteLater();
    }
    
    // Show empty label if no peers
    m_emptyLabel->setVisible(m_peerWidgets.isEmpty());
}

void LobbyPage::onPeerUpdated(Peer* peer)
{
    if (m_peerWidgets.contains(peer->id())) {
        m_peerWidgets[peer->id()]->updateDisplay();
    }
}

void LobbyPage::onDisplayNameChanged()
{
    QString newName = m_displayNameEdit->text().trimmed();
    if (!newName.isEmpty() && newName != m_peerManager->displayName()) {
        m_peerManager->setDisplayName(newName);
    }
}

void LobbyPage::updatePeerList()
{
    // Clear existing widgets
    for (PeerWidget* widget : m_peerWidgets.values()) {
        m_peersLayout->removeWidget(widget);
        widget->deleteLater();
    }
    m_peerWidgets.clear();
    
    // Add current peers
    for (Peer* peer : m_peerManager->peers()) {
        onPeerAdded(peer);
    }
    
    m_emptyLabel->setVisible(m_peerWidgets.isEmpty());
}

} // namespace Witra
