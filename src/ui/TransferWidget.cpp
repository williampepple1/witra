#include "TransferWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QLocale>

namespace Witra {

TransferWidget::TransferWidget(TransferItem* transfer, TransferManager* transferManager,
                               QWidget* parent)
    : QWidget(parent)
    , m_transfer(transfer)
    , m_transferManager(transferManager)
    , m_iconLabel(nullptr)
    , m_nameLabel(nullptr)
    , m_peerLabel(nullptr)
    , m_sizeLabel(nullptr)
    , m_speedLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr)
    , m_cancelButton(nullptr)
    , m_openButton(nullptr)
{
    setupUi();
    applyStyles();
    updateDisplay();
    
    connect(m_transfer, &TransferItem::progressChanged, this, &TransferWidget::updateDisplay);
    connect(m_transfer, &TransferItem::statusChanged, this, &TransferWidget::updateDisplay);
}

void TransferWidget::setupUi()
{
    setObjectName("transferWidget");
    setMinimumHeight(90);
    
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(16, 14, 16, 14);
    mainLayout->setSpacing(16);
    
    // File icon
    m_iconLabel = new QLabel();
    m_iconLabel->setObjectName("fileIcon");
    m_iconLabel->setFixedSize(48, 48);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_iconLabel);
    
    // File info
    QVBoxLayout* infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);
    
    // Name row
    QHBoxLayout* nameRow = new QHBoxLayout();
    nameRow->setSpacing(8);
    
    m_nameLabel = new QLabel();
    m_nameLabel->setObjectName("fileName");
    
    m_statusLabel = new QLabel();
    m_statusLabel->setObjectName("statusLabel");
    
    nameRow->addWidget(m_nameLabel);
    nameRow->addWidget(m_statusLabel);
    nameRow->addStretch();
    
    // Details row
    QHBoxLayout* detailsRow = new QHBoxLayout();
    detailsRow->setSpacing(12);
    
    m_peerLabel = new QLabel();
    m_peerLabel->setObjectName("peerLabel");
    
    m_sizeLabel = new QLabel();
    m_sizeLabel->setObjectName("sizeLabel");
    
    m_speedLabel = new QLabel();
    m_speedLabel->setObjectName("speedLabel");
    
    detailsRow->addWidget(m_peerLabel);
    detailsRow->addWidget(new QLabel("•"));
    detailsRow->addWidget(m_sizeLabel);
    detailsRow->addWidget(m_speedLabel);
    detailsRow->addStretch();
    
    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setObjectName("transferProgress");
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(4);
    
    infoLayout->addLayout(nameRow);
    infoLayout->addLayout(detailsRow);
    infoLayout->addSpacing(4);
    infoLayout->addWidget(m_progressBar);
    
    mainLayout->addLayout(infoLayout, 1);
    
    // Actions
    QVBoxLayout* actionLayout = new QVBoxLayout();
    actionLayout->setAlignment(Qt::AlignCenter);
    
    m_cancelButton = new QPushButton("Cancel");
    m_cancelButton->setObjectName("cancelButton");
    connect(m_cancelButton, &QPushButton::clicked, this, &TransferWidget::onCancelClicked);
    
    m_openButton = new QPushButton("Open Folder");
    m_openButton->setObjectName("openButton");
    connect(m_openButton, &QPushButton::clicked, this, &TransferWidget::onOpenFolderClicked);
    
    actionLayout->addWidget(m_cancelButton);
    actionLayout->addWidget(m_openButton);
    
    mainLayout->addLayout(actionLayout);
}

void TransferWidget::applyStyles()
{
    setStyleSheet(R"(
        #transferWidget {
            background-color: #161B22;
            border: 1px solid #30363D;
            border-radius: 12px;
        }
        
        #fileIcon {
            background-color: #21262D;
            border-radius: 10px;
            font-size: 24px;
        }
        
        #fileName {
            font-family: 'Segoe UI', sans-serif;
            font-size: 14px;
            font-weight: 600;
            color: #F0F6FC;
        }
        
        #statusLabel {
            font-family: 'Segoe UI', sans-serif;
            font-size: 11px;
            font-weight: 500;
            padding: 2px 8px;
            border-radius: 4px;
        }
        
        #peerLabel, #sizeLabel, QLabel {
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
            color: #8B949E;
        }
        
        #speedLabel {
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
            color: #00D9FF;
        }
        
        #transferProgress {
            background-color: #21262D;
            border: none;
            border-radius: 2px;
        }
        
        #transferProgress::chunk {
            background-color: #00D9FF;
            border-radius: 2px;
        }
        
        #cancelButton {
            background-color: transparent;
            border: 1px solid #F85149;
            border-radius: 6px;
            padding: 6px 14px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
            color: #F85149;
        }
        
        #cancelButton:hover {
            background-color: #F85149;
            color: white;
        }
        
        #openButton {
            background-color: #238636;
            border: none;
            border-radius: 6px;
            padding: 6px 14px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 12px;
            color: white;
        }
        
        #openButton:hover {
            background-color: #2EA043;
        }
    )");
}

void TransferWidget::updateDisplay()
{
    // File icon
    m_iconLabel->setText(getFileIcon());
    
    // File name
    m_nameLabel->setText(m_transfer->fileName());
    
    // Direction and peer
    QString direction = m_transfer->direction() == TransferItem::Direction::Incoming 
                        ? "from" : "to";
    m_peerLabel->setText(QString("%1 %2").arg(direction).arg(m_transfer->peerName()));
    
    // Size
    qint64 transferred = m_transfer->transferredSize();
    qint64 total = m_transfer->totalSize();
    m_sizeLabel->setText(QString("%1 / %2")
                         .arg(formatSize(transferred))
                         .arg(formatSize(total)));
    
    // Speed
    QString speed = m_transfer->speedString();
    m_speedLabel->setText(speed.isEmpty() ? "" : QString("• %1").arg(speed));
    m_speedLabel->setVisible(!speed.isEmpty());
    
    // Progress
    m_progressBar->setValue(static_cast<int>(m_transfer->progress()));
    
    // Status
    TransferItem::Status status = m_transfer->status();
    m_statusLabel->setText(m_transfer->statusString());
    
    QString statusStyle;
    switch (status) {
        case TransferItem::Status::InProgress:
            statusStyle = "background-color: #0D419D; color: #58A6FF;";
            break;
        case TransferItem::Status::Completed:
            statusStyle = "background-color: #0E4429; color: #3FB950;";
            break;
        case TransferItem::Status::Failed:
        case TransferItem::Status::Cancelled:
            statusStyle = "background-color: #490202; color: #F85149;";
            break;
        default:
            statusStyle = "background-color: #3D2A00; color: #D29922;";
            break;
    }
    m_statusLabel->setStyleSheet(QString(
        "font-family: 'Segoe UI'; font-size: 11px; font-weight: 500; "
        "padding: 2px 8px; border-radius: 4px; %1"
    ).arg(statusStyle));
    
    // Progress bar color
    QString progressColor = "#00D9FF";
    if (status == TransferItem::Status::Completed) {
        progressColor = "#238636";
    } else if (status == TransferItem::Status::Failed || 
               status == TransferItem::Status::Cancelled) {
        progressColor = "#F85149";
    }
    m_progressBar->setStyleSheet(QString(
        "#transferProgress { background-color: #21262D; border: none; border-radius: 2px; }"
        "#transferProgress::chunk { background-color: %1; border-radius: 2px; }"
    ).arg(progressColor));
    
    // Button visibility
    bool isActive = status == TransferItem::Status::InProgress || 
                    status == TransferItem::Status::Pending;
    bool isComplete = status == TransferItem::Status::Completed;
    bool isIncoming = m_transfer->direction() == TransferItem::Direction::Incoming;
    
    m_cancelButton->setVisible(isActive);
    m_openButton->setVisible(isComplete && isIncoming);
}

QString TransferWidget::formatSize(qint64 bytes) const
{
    QLocale locale;
    if (bytes >= 1024LL * 1024 * 1024) {
        return QString("%1 GB").arg(locale.toString(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2));
    } else if (bytes >= 1024 * 1024) {
        return QString("%1 MB").arg(locale.toString(bytes / (1024.0 * 1024.0), 'f', 2));
    } else if (bytes >= 1024) {
        return QString("%1 KB").arg(locale.toString(bytes / 1024.0, 'f', 1));
    } else {
        return QString("%1 B").arg(locale.toString(bytes));
    }
}

QString TransferWidget::getFileIcon() const
{
    QString fileName = m_transfer->fileName().toLower();
    
    if (fileName.endsWith(".jpg") || fileName.endsWith(".jpeg") || 
        fileName.endsWith(".png") || fileName.endsWith(".gif") ||
        fileName.endsWith(".webp") || fileName.endsWith(".bmp")) {
        return "🖼️";
    } else if (fileName.endsWith(".mp4") || fileName.endsWith(".avi") ||
               fileName.endsWith(".mkv") || fileName.endsWith(".mov") ||
               fileName.endsWith(".wmv")) {
        return "🎬";
    } else if (fileName.endsWith(".mp3") || fileName.endsWith(".wav") ||
               fileName.endsWith(".flac") || fileName.endsWith(".aac") ||
               fileName.endsWith(".ogg")) {
        return "🎵";
    } else if (fileName.endsWith(".pdf")) {
        return "📄";
    } else if (fileName.endsWith(".zip") || fileName.endsWith(".rar") ||
               fileName.endsWith(".7z") || fileName.endsWith(".tar") ||
               fileName.endsWith(".gz")) {
        return "📦";
    } else if (fileName.endsWith(".doc") || fileName.endsWith(".docx") ||
               fileName.endsWith(".txt") || fileName.endsWith(".rtf")) {
        return "📝";
    } else if (fileName.endsWith(".xls") || fileName.endsWith(".xlsx") ||
               fileName.endsWith(".csv")) {
        return "📊";
    } else if (fileName.endsWith(".ppt") || fileName.endsWith(".pptx")) {
        return "📽️";
    } else if (fileName.endsWith(".exe") || fileName.endsWith(".msi")) {
        return "⚙️";
    } else {
        return "📁";
    }
}

void TransferWidget::onCancelClicked()
{
    m_transferManager->cancelTransfer(m_transfer->id());
}

void TransferWidget::onOpenFolderClicked()
{
    QString filePath = m_transfer->filePath();
    if (filePath.isEmpty()) {
        filePath = m_transferManager->downloadPath();
    }
    
    QFileInfo fileInfo(filePath);
    QString folderPath = fileInfo.isDir() ? filePath : fileInfo.absolutePath();
    
    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
}

} // namespace Witra
