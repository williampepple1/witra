#include "TransferPage.h"
#include "TransferWidget.h"

#include <QScrollArea>
#include <QPushButton>
#include <QHBoxLayout>

namespace Witra {

TransferPage::TransferPage(TransferManager* transferManager, QWidget* parent)
    : QWidget(parent)
    , m_transferManager(transferManager)
    , m_transfersContainer(nullptr)
    , m_transfersLayout(nullptr)
    , m_emptyLabel(nullptr)
{
    setupUi();
    applyStyles();
    
    // Connect signals
    connect(m_transferManager, &TransferManager::transferAdded, 
            this, &TransferPage::onTransferAdded);
    connect(m_transferManager, &TransferManager::transferUpdated, 
            this, &TransferPage::onTransferUpdated);
    connect(m_transferManager, &TransferManager::transferRemoved, 
            this, &TransferPage::onTransferRemoved);
    
    // Load existing transfers
    updateTransferList();
}

void TransferPage::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(32, 24, 32, 24);
    mainLayout->setSpacing(24);
    
    // Header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QLabel* title = new QLabel("File Transfers");
    title->setObjectName("sectionTitle");
    
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    
    QPushButton* clearBtn = new QPushButton("Clear Completed");
    clearBtn->setObjectName("clearButton");
    connect(clearBtn, &QPushButton::clicked, this, &TransferPage::clearCompleted);
    headerLayout->addWidget(clearBtn);
    
    mainLayout->addLayout(headerLayout);
    
    // Stats cards
    QHBoxLayout* statsLayout = new QHBoxLayout();
    statsLayout->setSpacing(16);
    
    auto createStatCard = [](const QString& title, const QString& value, 
                             const QString& color) -> QWidget* {
        QWidget* card = new QWidget();
        card->setObjectName("statCard");
        
        QVBoxLayout* layout = new QVBoxLayout(card);
        layout->setContentsMargins(20, 16, 20, 16);
        layout->setSpacing(4);
        
        QLabel* valueLabel = new QLabel(value);
        valueLabel->setObjectName("statValue");
        valueLabel->setStyleSheet(QString("color: %1;").arg(color));
        
        QLabel* titleLabel = new QLabel(title);
        titleLabel->setObjectName("statTitle");
        
        layout->addWidget(valueLabel);
        layout->addWidget(titleLabel);
        
        return card;
    };
    
    QWidget* activeCard = createStatCard("Active", "0", "#00D9FF");
    QWidget* completedCard = createStatCard("Completed", "0", "#238636");
    QWidget* failedCard = createStatCard("Failed", "0", "#F85149");
    
    statsLayout->addWidget(activeCard);
    statsLayout->addWidget(completedCard);
    statsLayout->addWidget(failedCard);
    statsLayout->addStretch(1);
    
    // Update stats periodically
    QTimer* statsTimer = new QTimer(this);
    connect(statsTimer, &QTimer::timeout, this, [this, activeCard, completedCard, failedCard]() {
        int active = 0, completed = 0, failed = 0;
        
        for (TransferItem* item : m_transferManager->transfers()) {
            switch (item->status()) {
                case TransferItem::Status::InProgress:
                case TransferItem::Status::Pending:
                    active++;
                    break;
                case TransferItem::Status::Completed:
                    completed++;
                    break;
                case TransferItem::Status::Failed:
                case TransferItem::Status::Cancelled:
                    failed++;
                    break;
            }
        }
        
        activeCard->findChild<QLabel*>("statValue")->setText(QString::number(active));
        completedCard->findChild<QLabel*>("statValue")->setText(QString::number(completed));
        failedCard->findChild<QLabel*>("statValue")->setText(QString::number(failed));
    });
    statsTimer->start(1000);
    
    mainLayout->addLayout(statsLayout);
    
    // Transfers list
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setObjectName("transfersScroll");
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    m_transfersContainer = new QWidget();
    m_transfersContainer->setObjectName("transfersContainer");
    
    m_transfersLayout = new QVBoxLayout(m_transfersContainer);
    m_transfersLayout->setContentsMargins(0, 0, 0, 0);
    m_transfersLayout->setSpacing(12);
    m_transfersLayout->addStretch();
    
    // Empty state
    m_emptyLabel = new QLabel();
    m_emptyLabel->setObjectName("emptyLabel");
    m_emptyLabel->setText(
        "<div style='text-align: center;'>"
        "<p style='font-size: 48px; margin-bottom: 16px;'>📁</p>"
        "<p style='font-size: 16px; color: #8B949E; margin-bottom: 8px;'>No transfers yet</p>"
        "<p style='font-size: 13px; color: #484F58;'>Connect to a device and send files<br/>or drag & drop files onto the window</p>"
        "</div>"
    );
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_transfersLayout->insertWidget(0, m_emptyLabel);
    
    scrollArea->setWidget(m_transfersContainer);
    mainLayout->addWidget(scrollArea, 1);
}

void TransferPage::applyStyles()
{
    setStyleSheet(R"(
        TransferPage {
            background-color: #0D1117;
        }
        
        #sectionTitle {
            font-family: 'Segoe UI', sans-serif;
            font-size: 18px;
            font-weight: 600;
            color: #F0F6FC;
        }
        
        #clearButton {
            background-color: transparent;
            border: 1px solid #30363D;
            border-radius: 8px;
            padding: 8px 16px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            color: #8B949E;
        }
        
        #clearButton:hover {
            background-color: #21262D;
            border-color: #484F58;
            color: #F0F6FC;
        }
        
        #statCard {
            background-color: #161B22;
            border: 1px solid #30363D;
            border-radius: 12px;
            min-width: 140px;
        }
        
        #statValue {
            font-family: 'Segoe UI', sans-serif;
            font-size: 28px;
            font-weight: 700;
        }
        
        #statTitle {
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            color: #8B949E;
        }
        
        #transfersScroll {
            background-color: transparent;
        }
        
        #transfersContainer {
            background-color: transparent;
        }
        
        #emptyLabel {
            padding: 48px;
        }
    )");
}

void TransferPage::onTransferAdded(TransferItem* transfer)
{
    if (m_transferWidgets.contains(transfer->id())) return;
    
    TransferWidget* widget = new TransferWidget(transfer, m_transferManager, this);
    m_transferWidgets[transfer->id()] = widget;
    
    // Insert at top (before stretch)
    m_transfersLayout->insertWidget(0, widget);
    
    // Hide empty label
    m_emptyLabel->setVisible(false);
}

void TransferPage::onTransferUpdated(TransferItem* transfer)
{
    if (m_transferWidgets.contains(transfer->id())) {
        m_transferWidgets[transfer->id()]->updateDisplay();
    }
}

void TransferPage::onTransferRemoved(const QString& transferId)
{
    if (m_transferWidgets.contains(transferId)) {
        TransferWidget* widget = m_transferWidgets.take(transferId);
        m_transfersLayout->removeWidget(widget);
        widget->deleteLater();
    }
    
    m_emptyLabel->setVisible(m_transferWidgets.isEmpty());
}

void TransferPage::updateTransferList()
{
    for (TransferItem* transfer : m_transferManager->transfers()) {
        onTransferAdded(transfer);
    }
    
    m_emptyLabel->setVisible(m_transferWidgets.isEmpty());
}

void TransferPage::clearCompleted()
{
    QStringList toRemove;
    
    for (auto it = m_transferWidgets.begin(); it != m_transferWidgets.end(); ++it) {
        TransferItem* item = m_transferManager->transfer(it.key());
        if (item && (item->status() == TransferItem::Status::Completed ||
                     item->status() == TransferItem::Status::Failed ||
                     item->status() == TransferItem::Status::Cancelled)) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString& id : toRemove) {
        onTransferRemoved(id);
    }
}

} // namespace Witra
