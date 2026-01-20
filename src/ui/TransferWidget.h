#ifndef TRANSFERWIDGET_H
#define TRANSFERWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include "core/TransferItem.h"
#include "core/TransferManager.h"

namespace Witra {

class TransferWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit TransferWidget(TransferItem* transfer, TransferManager* transferManager,
                            QWidget* parent = nullptr);
    
    void updateDisplay();
    
private slots:
    void onCancelClicked();
    void onOpenFolderClicked();
    
private:
    void setupUi();
    void applyStyles();
    QString formatSize(qint64 bytes) const;
    QString getFileIcon() const;
    
    TransferItem* m_transfer;
    TransferManager* m_transferManager;
    
    QLabel* m_iconLabel;
    QLabel* m_nameLabel;
    QLabel* m_peerLabel;
    QLabel* m_sizeLabel;
    QLabel* m_speedLabel;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QPushButton* m_cancelButton;
    QPushButton* m_openButton;
};

} // namespace Witra

#endif // TRANSFERWIDGET_H
