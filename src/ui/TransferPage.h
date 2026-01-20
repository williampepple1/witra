#ifndef TRANSFERPAGE_H
#define TRANSFERPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include "core/TransferManager.h"

namespace Witra {

class TransferWidget;

class TransferPage : public QWidget {
    Q_OBJECT
    
public:
    explicit TransferPage(TransferManager* transferManager, QWidget* parent = nullptr);
    
private slots:
    void onTransferAdded(TransferItem* transfer);
    void onTransferUpdated(TransferItem* transfer);
    void onTransferRemoved(const QString& transferId);
    void updateTransferList();
    void clearCompleted();
    
private:
    void setupUi();
    void applyStyles();
    
    TransferManager* m_transferManager;
    
    QWidget* m_transfersContainer;
    QVBoxLayout* m_transfersLayout;
    QLabel* m_emptyLabel;
    
    QMap<QString, TransferWidget*> m_transferWidgets;
};

} // namespace Witra

#endif // TRANSFERPAGE_H
