#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>

namespace Witra {

class ConnectionDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit ConnectionDialog(const QString& senderName, QWidget* parent = nullptr);
    
signals:
    void accepted();
    void rejected();
    
private:
    void setupUi();
    void applyStyles();
    
    QString m_senderName;
};

} // namespace Witra

#endif // CONNECTIONDIALOG_H
