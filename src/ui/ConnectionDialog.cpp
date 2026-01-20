#include "ConnectionDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

namespace Witra {

ConnectionDialog::ConnectionDialog(const QString& senderName, QWidget* parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_senderName(senderName)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(false);
    
    setupUi();
    applyStyles();
    
    // Position at top-right of parent
    if (parent) {
        QPoint parentPos = parent->mapToGlobal(QPoint(parent->width(), 0));
        move(parentPos.x() - width() - 20, parentPos.y() + 80);
    }
    
    // Fade in animation
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(effect);
    
    QPropertyAnimation* fadeIn = new QPropertyAnimation(effect, "opacity");
    fadeIn->setDuration(200);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    
    // Auto-close after 30 seconds
    QTimer::singleShot(30000, this, &QDialog::reject);
}

void ConnectionDialog::setupUi()
{
    setFixedSize(340, 160);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    QWidget* container = new QWidget();
    container->setObjectName("dialogContainer");
    
    QVBoxLayout* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(20, 18, 20, 18);
    containerLayout->setSpacing(16);
    
    // Header
    QLabel* titleLabel = new QLabel("Connection Request");
    titleLabel->setObjectName("dialogTitle");
    
    // Message
    QLabel* messageLabel = new QLabel(
        QString("<b>%1</b> wants to connect and share files with you.")
        .arg(m_senderName)
    );
    messageLabel->setObjectName("dialogMessage");
    messageLabel->setWordWrap(true);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    
    QPushButton* rejectBtn = new QPushButton("Decline");
    rejectBtn->setObjectName("declineButton");
    connect(rejectBtn, &QPushButton::clicked, this, [this]() {
        emit rejected();
        close();
    });
    
    QPushButton* acceptBtn = new QPushButton("Accept");
    acceptBtn->setObjectName("acceptButton");
    connect(acceptBtn, &QPushButton::clicked, this, [this]() {
        emit accepted();
        close();
    });
    
    buttonLayout->addWidget(rejectBtn);
    buttonLayout->addWidget(acceptBtn);
    
    containerLayout->addWidget(titleLabel);
    containerLayout->addWidget(messageLabel);
    containerLayout->addStretch();
    containerLayout->addLayout(buttonLayout);
    
    mainLayout->addWidget(container);
}

void ConnectionDialog::applyStyles()
{
    setStyleSheet(R"(
        #dialogContainer {
            background-color: #161B22;
            border: 1px solid #30363D;
            border-radius: 16px;
        }
        
        #dialogTitle {
            font-family: 'Segoe UI', sans-serif;
            font-size: 15px;
            font-weight: 600;
            color: #F0F6FC;
        }
        
        #dialogMessage {
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            color: #8B949E;
            line-height: 1.4;
        }
        
        #dialogMessage b {
            color: #00D9FF;
        }
        
        #declineButton {
            background-color: transparent;
            border: 1px solid #30363D;
            border-radius: 8px;
            padding: 10px 24px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            font-weight: 500;
            color: #8B949E;
            min-width: 100px;
        }
        
        #declineButton:hover {
            background-color: #21262D;
            border-color: #484F58;
            color: #F0F6FC;
        }
        
        #acceptButton {
            background-color: #238636;
            border: none;
            border-radius: 8px;
            padding: 10px 24px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 13px;
            font-weight: 600;
            color: white;
            min-width: 100px;
        }
        
        #acceptButton:hover {
            background-color: #2EA043;
        }
    )");
}

} // namespace Witra
