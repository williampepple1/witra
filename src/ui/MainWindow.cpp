#include "MainWindow.h"
#include "LobbyPage.h"
#include "TransferPage.h"
#include "ConnectionDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QApplication>
#include <QSettings>
#include <QScreen>

namespace Witra {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_peerManager(new PeerManager(this))
    , m_transferManager(new TransferManager(m_peerManager, this))
    , m_stackedWidget(new QStackedWidget(this))
    , m_lobbyPage(nullptr)
    , m_transferPage(nullptr)
    , m_trayIcon(nullptr)
{
    // Set application icon
    QIcon appIcon(":/icons/app.svg");
    setWindowIcon(appIcon);
    qApp->setWindowIcon(appIcon);
    
    setupUi();
    setupTrayIcon();
    applyStyles();
    
    // Connect signals
    connect(m_transferManager, &TransferManager::connectionRequestReceived,
            this, &MainWindow::onConnectionRequestReceived);
    connect(m_transferManager, &TransferManager::error,
            this, &MainWindow::onError);
    connect(m_peerManager, &PeerManager::error,
            this, &MainWindow::onError);
    
    // Start services
    m_peerManager->start();
    m_transferManager->start();
    
    // Enable drag and drop
    setAcceptDrops(true);
}

MainWindow::~MainWindow()
{
    m_transferManager->stop();
    m_peerManager->stop();
}

void MainWindow::setupUi()
{
    setWindowTitle("Witra - Wireless Transfer");
    setMinimumSize(900, 650);
    
    // Center window on screen
    QSettings settings;
    if (settings.contains("window/geometry")) {
        restoreGeometry(settings.value("window/geometry").toByteArray());
    } else {
        resize(1000, 700);
        move(screen()->geometry().center() - rect().center());
    }
    
    // Main container
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Header
    QWidget* header = new QWidget();
    header->setObjectName("header");
    header->setFixedHeight(70);
    
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 0, 24, 0);
    
    // Logo and title
    QLabel* logo = new QLabel();
    logo->setObjectName("logo");
    logo->setText("◇");
    logo->setStyleSheet("font-size: 28px; color: #00D9FF;");
    
    QLabel* title = new QLabel("Witra");
    title->setObjectName("title");
    
    QLabel* subtitle = new QLabel("Wireless Transfer");
    subtitle->setObjectName("subtitle");
    
    QVBoxLayout* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(0);
    titleLayout->addWidget(title);
    titleLayout->addWidget(subtitle);
    
    headerLayout->addWidget(logo);
    headerLayout->addSpacing(12);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    
    // Navigation buttons
    QPushButton* lobbyBtn = new QPushButton("Network");
    lobbyBtn->setObjectName("navButton");
    lobbyBtn->setCheckable(true);
    lobbyBtn->setChecked(true);
    
    QPushButton* transfersBtn = new QPushButton("Transfers");
    transfersBtn->setObjectName("navButton");
    transfersBtn->setCheckable(true);
    
    connect(lobbyBtn, &QPushButton::clicked, this, [this, lobbyBtn, transfersBtn]() {
        lobbyBtn->setChecked(true);
        transfersBtn->setChecked(false);
        showLobbyPage();
    });
    
    connect(transfersBtn, &QPushButton::clicked, this, [this, lobbyBtn, transfersBtn]() {
        lobbyBtn->setChecked(false);
        transfersBtn->setChecked(true);
        showTransferPage();
    });
    
    headerLayout->addWidget(lobbyBtn);
    headerLayout->addSpacing(8);
    headerLayout->addWidget(transfersBtn);
    
    mainLayout->addWidget(header);
    
    // Divider
    QFrame* divider = new QFrame();
    divider->setFrameShape(QFrame::HLine);
    divider->setObjectName("divider");
    mainLayout->addWidget(divider);
    
    // Pages
    m_lobbyPage = new LobbyPage(m_peerManager, m_transferManager, this);
    m_transferPage = new TransferPage(m_transferManager, this);
    
    m_stackedWidget->addWidget(m_lobbyPage);
    m_stackedWidget->addWidget(m_transferPage);
    
    mainLayout->addWidget(m_stackedWidget, 1);
    
    // Status bar
    QWidget* statusBar = new QWidget();
    statusBar->setObjectName("statusBar");
    statusBar->setFixedHeight(36);
    
    QHBoxLayout* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(24, 0, 24, 0);
    
    QLabel* statusLabel = new QLabel();
    statusLabel->setObjectName("statusLabel");
    
    // Update status periodically
    auto updateStatus = [this, statusLabel]() {
        int peerCount = m_peerManager->peerCount();
        int transferCount = 0;
        for (TransferItem* item : m_transferManager->transfers()) {
            if (item->status() == TransferItem::Status::InProgress) {
                transferCount++;
            }
        }
        
        QString status = QString("%1 device%2 on network")
            .arg(peerCount)
            .arg(peerCount == 1 ? "" : "s");
        
        if (transferCount > 0) {
            status += QString(" • %1 active transfer%2")
                .arg(transferCount)
                .arg(transferCount == 1 ? "" : "s");
        }
        
        statusLabel->setText(status);
    };
    
    QTimer* statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, updateStatus);
    statusTimer->start(1000);
    updateStatus();
    
    statusLayout->addWidget(statusLabel);
    statusLayout->addStretch();
    
    QLabel* versionLabel = new QLabel("v1.0.0");
    versionLabel->setObjectName("versionLabel");
    statusLayout->addWidget(versionLabel);
    
    mainLayout->addWidget(statusBar);
}

void MainWindow::setupTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icons/app.svg"));
    m_trayIcon->setToolTip("Witra - Wireless Transfer");
    
    QMenu* trayMenu = new QMenu(this);
    
    QAction* showAction = trayMenu->addAction("Show Witra");
    connect(showAction, &QAction::triggered, this, &MainWindow::show);
    
    trayMenu->addSeparator();
    
    QAction* quitAction = trayMenu->addAction("Quit");
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    
    m_trayIcon->setContextMenu(trayMenu);
    
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, 
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            show();
            raise();
            activateWindow();
        }
    });
    
    m_trayIcon->show();
}

void MainWindow::applyStyles()
{
    setStyleSheet(R"(
        QMainWindow {
            background-color: #0D1117;
        }
        
        #header {
            background-color: #161B22;
            border-bottom: 1px solid #30363D;
        }
        
        #title {
            font-family: 'Segoe UI', 'SF Pro Display', sans-serif;
            font-size: 20px;
            font-weight: 600;
            color: #F0F6FC;
            letter-spacing: -0.5px;
        }
        
        #subtitle {
            font-family: 'Segoe UI', sans-serif;
            font-size: 11px;
            color: #8B949E;
            letter-spacing: 0.5px;
        }
        
        #navButton {
            background-color: transparent;
            border: none;
            border-radius: 8px;
            padding: 10px 20px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            font-weight: 500;
            color: #8B949E;
        }
        
        #navButton:hover {
            background-color: #21262D;
            color: #F0F6FC;
        }
        
        #navButton:checked {
            background-color: #00D9FF;
            color: #0D1117;
        }
        
        #divider {
            background-color: #30363D;
            border: none;
            height: 1px;
        }
        
        #statusBar {
            background-color: #161B22;
            border-top: 1px solid #30363D;
        }
        
        #statusLabel {
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
            color: #8B949E;
        }
        
        #versionLabel {
            font-family: 'Segoe UI', sans-serif;
            font-size: 11px;
            color: #484F58;
        }
        
        QScrollBar:vertical {
            background-color: #0D1117;
            width: 10px;
            margin: 0;
        }
        
        QScrollBar::handle:vertical {
            background-color: #30363D;
            border-radius: 5px;
            min-height: 30px;
        }
        
        QScrollBar::handle:vertical:hover {
            background-color: #484F58;
        }
        
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }
        
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: none;
        }
    )");
}

void MainWindow::showLobbyPage()
{
    m_stackedWidget->setCurrentWidget(m_lobbyPage);
}

void MainWindow::showTransferPage()
{
    m_stackedWidget->setCurrentWidget(m_transferPage);
}

void MainWindow::onConnectionRequestReceived(TransferSession* session, 
                                              const QString& senderName)
{
    ConnectionDialog* dialog = new ConnectionDialog(senderName, this);
    
    connect(dialog, &ConnectionDialog::accepted, this, [this, session]() {
        m_transferManager->acceptConnectionRequest(session);
    });
    
    connect(dialog, &ConnectionDialog::rejected, this, [this, session]() {
        m_transferManager->rejectConnectionRequest(session);
    });
    
    dialog->show();
    
    // Show notification
    if (m_trayIcon && !isActiveWindow()) {
        m_trayIcon->showMessage("Connection Request",
            QString("%1 wants to connect").arg(senderName),
            QSystemTrayIcon::Information, 5000);
    }
}

void MainWindow::onError(const QString& error)
{
    QMessageBox::warning(this, "Error", error);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    
    // Minimize to tray instead of closing
    if (m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    
    if (mimeData->hasUrls()) {
        QStringList paths;
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                paths.append(url.toLocalFile());
            }
        }
        
        if (!paths.isEmpty()) {
            // Switch to lobby to select a peer
            showLobbyPage();
            m_lobbyPage->setFilesToSend(paths);
        }
    }
}

} // namespace Witra
